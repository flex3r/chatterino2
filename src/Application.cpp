#include "Application.hpp"

#include "controllers/accounts/AccountController.hpp"
#include "controllers/commands/CommandController.hpp"
#include "controllers/highlights/HighlightController.hpp"
#include "controllers/ignores/IgnoreController.hpp"
#include "controllers/moderationactions/ModerationActions.hpp"
#include "controllers/notifications/NotificationController.hpp"
#include "controllers/taggedusers/TaggedUsersController.hpp"
#include "debug/Log.hpp"
#include "messages/MessageBuilder.hpp"
#include "providers/bttv/BttvEmotes.hpp"
#include "providers/chatterino/ChatterinoBadges.hpp"
#include "providers/ffz/FfzEmotes.hpp"
#include "providers/twitch/PubsubClient.hpp"
#include "providers/twitch/TwitchServer.hpp"
#include "singletons/Emotes.hpp"
#include "singletons/Fonts.hpp"
#include "singletons/Logging.hpp"
#include "singletons/NativeMessaging.hpp"
#include "singletons/Paths.hpp"
#include "singletons/Resources.hpp"
#include "singletons/Settings.hpp"
#include "singletons/Theme.hpp"
#include "singletons/Toasts.hpp"
#include "singletons/WindowManager.hpp"
#include "util/IsBigEndian.hpp"
#include "util/PostToThread.hpp"
#include "widgets/Window.hpp"

#include <atomic>

namespace chatterino {

static std::atomic<bool> isAppInitialized{false};

Application *Application::instance = nullptr;

// this class is responsible for handling the workflow of Chatterino
// It will create the instances of the major classes, and connect their signals
// to each other

Application::Application(Settings &_settings, Paths &_paths)
    : resources(&this->emplace<Resources2>())

    , themes(&this->emplace<Theme>())
    , fonts(&this->emplace<Fonts>())
    , emotes(&this->emplace<Emotes>())
    , windows(&this->emplace<WindowManager>())
    , toasts(&this->emplace<Toasts>())

    , accounts(&this->emplace<AccountController>())
    , commands(&this->emplace<CommandController>())
    , highlights(&this->emplace<HighlightController>())
    , notifications(&this->emplace<NotificationController>())
    , ignores(&this->emplace<IgnoreController>())
    , taggedUsers(&this->emplace<TaggedUsersController>())
    , moderationActions(&this->emplace<ModerationActions>())
    , twitch2(&this->emplace<TwitchServer>())
    , chatterinoBadges(&this->emplace<ChatterinoBadges>())
    , logging(&this->emplace<Logging>())

{
    this->instance = this;

    this->fonts->fontChanged.connect(
        [this]() { this->windows->layoutChannelViews(); });

    this->twitch.server = this->twitch2;
    this->twitch.pubsub = this->twitch2->pubsub;
}

void Application::initialize(Settings &settings, Paths &paths)
{
    assert(isAppInitialized == false);
    isAppInitialized = true;

    for (auto &singleton : this->singletons_) {
        singleton->initialize(settings, paths);
    }

    this->windows->updateWordTypeMask();

    this->initNm(paths);
    this->initPubsub();
}

int Application::run(QApplication &qtApp)
{
    assert(isAppInitialized);

    this->twitch.server->connect();

    this->windows->getMainWindow().show();

    return qtApp.exec();
}

void Application::save()
{
    for (auto &singleton : this->singletons_) {
        singleton->save();
    }
}

void Application::initNm(Paths &paths)
{
#ifdef Q_OS_WIN
#    ifdef QT_DEBUG
#        ifdef C_DEBUG_NM
    registerNmHost(paths);
    this->nmServer.start();
#        endif
#    else
    registerNmHost(paths);
    this->nmServer.start();
#    endif
#endif
}

void Application::initPubsub()
{
    this->twitch.pubsub->signals_.whisper.sent.connect([](const auto &msg) {
        log("WHISPER SENT LOL");  //
    });

    this->twitch.pubsub->signals_.whisper.received.connect([](const auto &msg) {
        log("WHISPER RECEIVED LOL");  //
    });

    this->twitch.pubsub->signals_.moderation.chatCleared.connect(
        [this](const auto &action) {
            auto chan =
                this->twitch.server->getChannelOrEmptyByID(action.roomID);
            if (chan->isEmpty()) {
                return;
            }

            QString text =
                QString("%1 cleared the chat").arg(action.source.name);

            auto msg = makeSystemMessage(text);
            postToThread([chan, msg] { chan->addMessage(msg); });
        });

    this->twitch.pubsub->signals_.moderation.modeChanged.connect(
        [this](const auto &action) {
            auto chan =
                this->twitch.server->getChannelOrEmptyByID(action.roomID);
            if (chan->isEmpty()) {
                return;
            }

            QString text =
                QString("%1 turned %2 %3 mode")  //
                    .arg(action.source.name)
                    .arg(action.state == ModeChangedAction::State::On ? "on"
                                                                      : "off")
                    .arg(action.getModeName());

            if (action.duration > 0) {
                text.append(" (" + QString::number(action.duration) +
                            " seconds)");
            }

            auto msg = makeSystemMessage(text);
            postToThread([chan, msg] { chan->addMessage(msg); });
        });

    this->twitch.pubsub->signals_.moderation.moderationStateChanged.connect(
        [this](const auto &action) {
            auto chan =
                this->twitch.server->getChannelOrEmptyByID(action.roomID);
            if (chan->isEmpty()) {
                return;
            }

            QString text;

            if (action.modded) {
                text = QString("%1 modded %2")
                           .arg(action.source.name, action.target.name);
            } else {
                text = QString("%1 unmodded %2")
                           .arg(action.source.name, action.target.name);
            }

            auto msg = makeSystemMessage(text);
            postToThread([chan, msg] { chan->addMessage(msg); });
        });

    this->twitch.pubsub->signals_.moderation.userBanned.connect(
        [&](const auto &action) {
            auto chan =
                this->twitch.server->getChannelOrEmptyByID(action.roomID);

            if (chan->isEmpty()) {
                return;
            }

            MessageBuilder msg(action);
            msg->flags.set(MessageFlag::PubSub);

            postToThread([chan, msg = msg.release()] {
                chan->addOrReplaceTimeout(msg);
            });
        });

    this->twitch.pubsub->signals_.moderation.userUnbanned.connect(
        [&](const auto &action) {
            auto chan =
                this->twitch.server->getChannelOrEmptyByID(action.roomID);

            if (chan->isEmpty()) {
                return;
            }

            auto msg = MessageBuilder(action).release();

            postToThread([chan, msg] { chan->addMessage(msg); });
        });

    this->twitch.pubsub->start();

    auto RequestModerationActions = [=]() {
        this->twitch.server->pubsub->unlistenAllModerationActions();
        // TODO(pajlada): Unlisten to all authed topics instead of only
        // moderation topics this->twitch.pubsub->UnlistenAllAuthedTopics();

        this->twitch.server->pubsub->listenToWhispers(
            this->accounts->twitch.getCurrent());  //
    };

    this->accounts->twitch.currentUserChanged.connect(RequestModerationActions);

    RequestModerationActions();
}

Application *getApp()
{
    assert(Application::instance != nullptr);

    return Application::instance;
}

}  // namespace chatterino
