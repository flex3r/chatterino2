#pragma once

#include "common/Channel.hpp"
#include "common/FlagsEnum.hpp"
#include "common/Singleton.hpp"
#include "widgets/splits/SplitContainer.hpp"

namespace chatterino {

class Settings;
class Paths;
class Window;
class SplitContainer;

enum class MessageElementFlag;
using MessageElementFlags = FlagsEnum<MessageElementFlag>;
enum class WindowType;

class WindowManager final : public Singleton
{
public:
    WindowManager();

    static void encodeChannel(IndirectChannel channel, QJsonObject &obj);
    static IndirectChannel decodeChannel(const QJsonObject &obj);

    static int clampUiScale(int scale);
    static float getUiScaleValue();
    static float getUiScaleValue(int scale);

    static const int uiScaleMin;
    static const int uiScaleMax;

    void showSettingsDialog();
    void showAccountSelectPopup(QPoint point);

    void layoutChannelViews(Channel *channel = nullptr);
    void forceLayoutChannelViews();
    void repaintVisibleChatWidgets(Channel *channel = nullptr);
    void repaintGifEmotes();

    Window &getMainWindow();
    Window &getSelectedWindow();
    Window &createWindow(WindowType type);

    int windowCount();
    Window *windowAt(int index);

    virtual void initialize(Settings &settings, Paths &paths) override;
    virtual void save() override;
    void closeAll();

    int getGeneration() const;
    void incGeneration();

    MessageElementFlags getWordFlags();
    void updateWordTypeMask();

    pajlada::Signals::NoArgSignal repaintGifs;
    pajlada::Signals::Signal<Channel *> layout;

    pajlada::Signals::NoArgSignal wordFlagsChanged;

private:
    void encodeNodeRecusively(SplitContainer::Node *node, QJsonObject &obj);

    bool initialized_ = false;

    std::atomic<int> generation_{0};

    std::vector<Window *> windows_;

    Window *mainWindow_{};
    Window *selectedWindow_{};

    MessageElementFlags wordFlags_{};
    pajlada::Settings::SettingListener wordFlagsListener_;
};

}  // namespace chatterino
