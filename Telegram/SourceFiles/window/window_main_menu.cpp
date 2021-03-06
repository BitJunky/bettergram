/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "window/window_main_menu.h"

#include "styles/style_window.h"
#include "styles/style_dialogs.h"
#include "window/themes/window_theme.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/menu.h"
#include "ui/special_buttons.h"
#include "ui/empty_userpic.h"
#include "mainwindow.h"
#include "storage/localstorage.h"
#include "boxes/about_box.h"
#include "boxes/peer_list_controllers.h"
#include "calls/calls_box_controller.h"
#include "lang/lang_keys.h"
#include "core/click_handler_types.h"
#include "observer_peer.h"
#include "auth_session.h"
#include "mainwidget.h"
#include "bettergram/bettergramservice.h"

namespace Window {

MainMenu::MainMenu(
	QWidget *parent,
	not_null<Controller*> controller)
: TWidget(parent)
, _controller(controller)
, _menu(this, st::mainMenu)
, _telegram(this, st::mainMenuTelegramLabel)
, _version(this, st::mainMenuVersionLabel)
, _manageSubscription(this, st::mainMenuManageSubscriptionLabel)
, _upgradeToBettergramPro(this, st::mainMenuUpgradeToBettergramProLabel)
, _noAdsPlusCustomPrices(this, st::mainMenuNoAdsPlusCustomPricesLabel) {
	setAttribute(Qt::WA_OpaquePaintEvent);

	_upgradeToBettergramPro->setVisible(false);
	_noAdsPlusCustomPrices->setVisible(false);

	subscribe(Global::RefSelfChanged(), [this] {
		checkSelf();
	});
	checkSelf();

	_nightThemeSwitch.setCallback([this] {
		if (const auto action = *_nightThemeAction) {
			const auto nightMode = Window::Theme::IsNightMode();
			if (action->isChecked() != nightMode) {
				Window::Theme::ToggleNightMode();
			}
		}
	});

	resize(st::mainMenuWidth, parentWidget()->height());
	_menu->setTriggeredCallback([](QAction *action, int actionTop, Ui::Menu::TriggeredSource source) {
		emit action->triggered();
	});
	refreshMenu();

	_telegram->setRichText(textcmdLink(1, qsl("Bettergram")));
	_telegram->setLink(1, std::make_shared<UrlClickHandler>(qsl("http://www.bettergram.io")));
	_version->setRichText(textcmdLink(1, lng_settings_current_version(lt_version, currentVersionText())) + QChar(' ') + QChar(8211) + QChar(' ') + textcmdLink(2, lang(lng_menu_about)));
	_version->setLink(1, std::make_shared<UrlClickHandler>(qsl("http://www.bettergram.io/changelog")));
	_version->setLink(2, std::make_shared<UrlClickHandler>(qsl("http://www.bettergram.io/about")));

	_manageSubscription->setRichText(textcmdLink(1, lang(lng_menu_manage_subscription)));
	_manageSubscription->setLink(1, std::make_shared<UrlClickHandler>(qsl("http://www.bettergram.io")));
	_manageSubscription->setVisible(false);

	Bettergram::BettergramService *settings = Bettergram::BettergramService::instance();
	subscribe(settings->isPaidObservable(), [this] { updateBettergramProText(); });

	updateBettergramProText();

	_noAdsPlusCustomPrices->setRichText(lang(lng_menu_no_ads_plus_custom_prices));

	subscribe(Auth().downloaderTaskFinished(), [this] { update(); });
	subscribe(Auth().downloaderTaskFinished(), [this] { update(); });
	subscribe(Notify::PeerUpdated(), Notify::PeerUpdatedHandler(Notify::PeerUpdate::Flag::UserPhoneChanged, [this](const Notify::PeerUpdate &update) {
		if (update.peer->isSelf()) {
			updatePhone();
		}
	}));
	subscribe(Global::RefPhoneCallsEnabledChanged(), [this] { refreshMenu(); });
	subscribe(Window::Theme::Background(), [this](const Window::Theme::BackgroundUpdate &update) {
		if (update.type == Window::Theme::BackgroundUpdate::Type::ApplyingTheme) {
			refreshMenu();
		}
	});

	updatePhone();
}

void MainMenu::refreshMenu() {
	_menu->clearActions();
	_menu->addAction(lang(lng_create_group_title), [] {
		App::wnd()->onShowNewGroup();
	}, &st::mainMenuNewGroup, &st::mainMenuNewGroupOver);
	_menu->addAction(lang(lng_create_channel_title), [] {
		App::wnd()->onShowNewChannel();
	}, &st::mainMenuNewChannel, &st::mainMenuNewChannelOver);
	_menu->addAction(lang(lng_menu_contacts), [] {
		Ui::show(Box<PeerListBox>(std::make_unique<ContactsBoxController>(), [](not_null<PeerListBox*> box) {
			box->addButton(langFactory(lng_close), [box] { box->closeBox(); });
			box->addLeftButton(langFactory(lng_profile_add_contact), [] { App::wnd()->onShowAddContact(); });
		}));
	}, &st::mainMenuContacts, &st::mainMenuContactsOver);
	if (Global::PhoneCallsEnabled()) {
		_menu->addAction(lang(lng_menu_calls), [] {
			Ui::show(Box<PeerListBox>(std::make_unique<Calls::BoxController>(), [](not_null<PeerListBox*> box) {
				box->addButton(langFactory(lng_close), [box] { box->closeBox(); });
			}));
		}, &st::mainMenuCalls, &st::mainMenuCallsOver);
	}
	_menu->addAction(lang(lng_menu_settings), [] {
		App::wnd()->showSettings();
	}, &st::mainMenuSettings, &st::mainMenuSettingsOver);

	_nightThemeAction = std::make_shared<QPointer<QAction>>(nullptr);
	auto action = _menu->addAction(lang(lng_menu_night_mode), [this] {
		if (auto action = *_nightThemeAction) {
			action->setChecked(!action->isChecked());
			_nightThemeSwitch.callOnce(st::mainMenu.itemToggle.duration);
		}
	}, &st::mainMenuNightMode, &st::mainMenuNightModeOver);
	*_nightThemeAction = action;
	action->setCheckable(true);
	action->setChecked(Window::Theme::IsNightMode());
	_menu->finishAnimating();

	updatePhone();
}

void MainMenu::updateBettergramProText()
{
	Bettergram::BettergramService *settings = Bettergram::BettergramService::instance();

	//TODO: bettergram: set correct links to bettergram pro labels

	if (settings->isPaid()) {
		_upgradeToBettergramPro->setRichText(textcmdLink(1, lang(lng_menu_bettergram_pro)));
		_upgradeToBettergramPro->setLink(1, std::make_shared<UrlClickHandler>(qsl("https://desktop.telegram.org")));
	} else {
		_upgradeToBettergramPro->setRichText(textcmdLink(1, lang(lng_menu_upgrade_to_bettergram_pro)));
		_upgradeToBettergramPro->setLink(1, std::make_shared<UrlClickHandler>(qsl("https://desktop.telegram.org")));
	}
}

void MainMenu::checkSelf() {
	if (auto self = App::self()) {
		auto showSelfChat = [] {
			if (auto self = App::self()) {
				App::main()->choosePeer(self->id, ShowAtUnreadMsgId);
			}
		};
		_userpicButton.create(
			this,
			_controller,
			self,
			Ui::UserpicButton::Role::Custom,
			st::mainMenuUserpic);
		_userpicButton->setClickedCallback(showSelfChat);
		_userpicButton->show();
		_cloudButton.create(this, st::mainMenuCloudButton);
		_cloudButton->setClickedCallback(showSelfChat);
		_cloudButton->show();
		update();
		updateControlsGeometry();
	} else {
		_userpicButton.destroy();
		_cloudButton.destroy();
	}
}

void MainMenu::resizeEvent(QResizeEvent *e) {
	_menu->setForceWidth(width());
	updateControlsGeometry();
}

void MainMenu::updateControlsGeometry() {
	if (_userpicButton) {
		_userpicButton->moveToLeft(st::mainMenuUserpicLeft, st::mainMenuUserpicTop);
	}
	if (_cloudButton) {
		_cloudButton->moveToRight(0, st::mainMenuCoverHeight - _cloudButton->height());
	}
	_menu->moveToLeft(0, st::mainMenuCoverHeight + st::mainMenuSkip);
	_telegram->moveToLeft(st::mainMenuFooterLeft, height() - st::mainMenuTelegramBottom - _telegram->height());
	_version->moveToLeft(st::mainMenuFooterLeft, height() - st::mainMenuVersionBottom - _version->height());

	// We use st::mainMenuVersionBottom in order to do not introduce new variable and to keep
	// spaces in coordinated view
	_noAdsPlusCustomPrices->moveToLeft(st::mainMenuFooterLeft, _telegram->y() - st::mainMenuVersionBottom - _upgradeToBettergramPro->height());
	_upgradeToBettergramPro->moveToLeft(st::mainMenuFooterLeft, _noAdsPlusCustomPrices->y() - _noAdsPlusCustomPrices->height());
	_manageSubscription->moveToLeft(st::mainMenuFooterLeft, _menu->y() + _menu->height() + st::mainMenuVersionBottom);
}

void MainMenu::updatePhone() {
	if (auto self = App::self()) {
		_phoneText = App::formatPhone(self->phone());
	} else {
		_phoneText = QString();
	}
	update();
}

void MainMenu::paintEvent(QPaintEvent *e) {
	Painter p(this);
	auto clip = e->rect();
	auto cover = QRect(0, 0, width(), st::mainMenuCoverHeight).intersected(clip);
	if (!cover.isEmpty()) {
		p.fillRect(cover, st::mainMenuCoverBg);
		p.setPen(st::mainMenuCoverFg);
		p.setFont(st::semiboldFont);
		if (auto self = App::self()) {
			self->nameText.drawLeftElided(p, st::mainMenuCoverTextLeft, st::mainMenuCoverNameTop, width() - 2 * st::mainMenuCoverTextLeft, width());
			p.setFont(st::normalFont);
			p.drawTextLeft(st::mainMenuCoverTextLeft, st::mainMenuCoverStatusTop, width(), _phoneText);
		}
		if (_cloudButton) {
			Ui::EmptyUserpic::PaintSavedMessages(
				p,
				_cloudButton->x() + (_cloudButton->width() - st::mainMenuCloudSize) / 2,
				_cloudButton->y() + (_cloudButton->height() - st::mainMenuCloudSize) / 2,
				width(),
				st::mainMenuCloudSize,
				st::mainMenuCloudBg,
				st::mainMenuCloudFg);
			//PainterHighQualityEnabler hq(p);
			//p.setPen(Qt::NoPen);
			//p.setBrush(st::mainMenuCloudBg);
			//auto cloudBg = QRect(
			//	_cloudButton->x() + (_cloudButton->width() - st::mainMenuCloudSize) / 2,
			//	_cloudButton->y() + (_cloudButton->height() - st::mainMenuCloudSize) / 2,
			//	st::mainMenuCloudSize,
			//	st::mainMenuCloudSize);
			//p.drawEllipse(cloudBg);
		}
	}
	auto other = QRect(0, st::mainMenuCoverHeight, width(), height() - st::mainMenuCoverHeight).intersected(clip);
	if (!other.isEmpty()) {
		p.fillRect(other, st::mainMenuBg);
	}
}

} // namespace Window
