#include "SettingsPage.hpp"

#include <QDebug>
#include <QPainter>

namespace chatterino {

SettingsPage::SettingsPage(const QString &name, const QString &iconResource)
    : name_(name)
    , iconResource_(iconResource)
{
}

const QString &SettingsPage::getName()
{
    return this->name_;
}

const QString &SettingsPage::getIconResource()
{
    return this->iconResource_;
}

void SettingsPage::cancel()
{
    this->onCancel_.invoke();
}

QCheckBox *SettingsPage::createCheckBox(
    const QString &text, pajlada::Settings::Setting<bool> &setting)
{
    QCheckBox *checkbox = new QCheckBox(text);

    // update when setting changes
    setting.connect(
        [checkbox](const bool &value, auto) {
            checkbox->setChecked(value);  //
        },
        this->managedConnections_);

    // update setting on toggle
    QObject::connect(checkbox, &QCheckBox::toggled, this,
                     [&setting](bool state) {
                         qDebug() << "update checkbox value";
                         setting = state;  //
                     });

    return checkbox;
}

QComboBox *SettingsPage::createComboBox(
    const QStringList &items, pajlada::Settings::Setting<QString> &setting)
{
    QComboBox *combo = new QComboBox();

    // update setting on toogle
    combo->addItems(items);

    // update when setting changes
    setting.connect(
        [combo](const QString &value, auto) { combo->setCurrentText(value); },
        this->managedConnections_);

    QObject::connect(
        combo, &QComboBox::currentTextChanged,
        [&setting](const QString &newValue) { setting = newValue; });

    return combo;
}

QLineEdit *SettingsPage::createLineEdit(
    pajlada::Settings::Setting<QString> &setting)
{
    QLineEdit *edit = new QLineEdit();

    edit->setText(setting);

    // update when setting changes
    QObject::connect(
        edit, &QLineEdit::textChanged,
        [&setting](const QString &newValue) { setting = newValue; });

    return edit;
}

QSpinBox *SettingsPage::createSpinBox(pajlada::Settings::Setting<int> &setting,
                                      int min, int max)
{
    QSpinBox *w = new QSpinBox;

    w->setMinimum(min);
    w->setMaximum(max);

    setting.connect([w](const int &value, auto) { w->setValue(value); });
    QObject::connect(w, QOverload<int>::of(&QSpinBox::valueChanged),
                     [&setting](int value) { setting.setValue(value); });

    return w;
}

}  // namespace chatterino
