#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#include <Geode/ui/TextInput.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include "State.hpp"

// =====================================================================
//  Popup: pedir nombre para guardar
// =====================================================================
class NamePopup : public geode::Popup {
protected:
    TextInput* m_input = nullptr;

    bool init() {
        if (!Popup::init(260.f, 140.f)) return false;
        this->setTitle("Guardar grabacion");

        m_input = TextInput::create(200.f, "Nombre", "bigFont.fnt");
        m_input->setMaxCharCount(24);
        m_mainLayer->addChildAtPosition(m_input, Anchor::Center, {0.f, 8.f});

        auto btn = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("Guardar"), this, menu_selector(NamePopup::onOk)
        );
        m_buttonMenu->addChildAtPosition(btn, Anchor::Bottom, {0.f, 28.f});
        return true;
    }

    void onOk(CCObject*) {
        auto name = m_input->getString();
        if (name.empty()) {
            ci::alertMsg("Click Indicator", "Ponle un nombre a la grabacion.");
            return;
        }
        if (ci::saveCurrent(name)) {
            Notification::create("Guardado: " + name, NotificationIcon::Success)->show();
            this->onClose(nullptr);
        } else {
            ci::alertMsg("Click Indicator", "No se pudo guardar (no hay clicks grabados).");
        }
    }

public:
    static NamePopup* create() {
        auto ret = new NamePopup();
        if (ret->init()) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }
};

// =====================================================================
//  Popup: lista de grabaciones guardadas (del nivel actual)
// =====================================================================
class SavedListPopup : public geode::Popup {
protected:
    bool init() {
        if (!Popup::init(260.f, 210.f)) return false;
        this->setTitle("Guardados");

        auto list = ci::listForLevel();

        if (list.empty()) {
            auto lbl = CCLabelBMFont::create("No hay grabaciones\npara este nivel", "bigFont.fnt");
            lbl->setScale(0.5f);
            lbl->setAlignment(kCCTextAlignmentCenter);
            m_mainLayer->addChildAtPosition(lbl, Anchor::Center);
            return true;
        }

        constexpr float W = 210.f;
        constexpr float H = 140.f;

        auto scroll = ScrollLayer::create({W, H});
        scroll->m_contentLayer->setLayout(
            ColumnLayout::create()
                ->setAxisReverse(true)
                ->setAxisAlignment(AxisAlignment::End)
                ->setAutoGrowAxis(H)
                ->setGap(6.f)
        );

        for (auto const& s : list) {
            auto row = CCMenu::create();
            row->setContentSize({W, 32.f});
            row->ignoreAnchorPointForPosition(false);

            bool selected = (s.path == ci::selectedPath);
            auto spr = ButtonSprite::create(
                s.name.c_str(), static_cast<int>(W - 20.f), true, "bigFont.fnt",
                selected ? "GJ_button_02.png" : "GJ_button_01.png", 28.f, 0.6f
            );
            auto btn = CCMenuItemExt::createSpriteExtra(spr, [this, s](CCMenuItemSpriteExtra*) {
                if (ci::selectRecording(s)) {
                    Notification::create("Seleccionado: " + s.name, NotificationIcon::Success)->show();
                } else {
                    ci::alertMsg("Click Indicator", "No se pudo leer esa grabacion.");
                }
                this->onClose(nullptr);
            });
            row->addChildAtPosition(btn, Anchor::Center);
            scroll->m_contentLayer->addChild(row);
        }
        scroll->m_contentLayer->updateLayout();
        scroll->moveToTop();

        m_mainLayer->addChildAtPosition(scroll, Anchor::Center, {-W / 2.f, -H / 2.f - 8.f});
        return true;
    }

public:
    static SavedListPopup* create() {
        auto ret = new SavedListPopup();
        if (ret->init()) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }
};

// =====================================================================
//  Popup: Personalizar
// =====================================================================
class PersonalizePopup : public geode::Popup {
protected:
    CCLabelBMFont* m_opLbl = nullptr;
    CCLabelBMFont* m_sizeLbl = nullptr;
    CCLabelBMFont* m_distLbl = nullptr;

    CCLabelBMFont* makeLabel(std::string const& text, float y, float x = 0.f) {
        auto lbl = CCLabelBMFont::create(text.c_str(), "bigFont.fnt");
        lbl->setScale(0.42f);
        m_mainLayer->addChildAtPosition(lbl, Anchor::Center, {x, y});
        return lbl;
    }

    Slider* makeSlider(float y, SEL_MenuHandler cb, float value) {
        auto slider = Slider::create(this, cb, 0.7f);
        slider->setValue(std::clamp(value, 0.f, 1.f));
        m_mainLayer->addChildAtPosition(slider, Anchor::Center, {0.f, y});
        return slider;
    }

    bool init() {
        if (!Popup::init(280.f, 310.f)) return false;
        this->setTitle("Personalizar");

        m_opLbl = makeLabel("", 98.f);
        makeSlider(78.f, menu_selector(PersonalizePopup::onOpacity), ci::getOpacity());

        m_sizeLbl = makeLabel("", 46.f);
        makeSlider(26.f, menu_selector(PersonalizePopup::onSize), ci::getTowerScale() - 0.5f);

        m_distLbl = makeLabel("", -6.f);
        makeSlider(-26.f, menu_selector(PersonalizePopup::onDist), (ci::getWarnBlocks() - 5.f) / 25.f);

        auto t1 = CCMenuItemToggler::createWithStandardSprites(this, menu_selector(PersonalizePopup::onTower), 0.7f);
        t1->toggle(ci::getShowTower());
        m_buttonMenu->addChildAtPosition(t1, Anchor::Center, {-100.f, -68.f});
        auto l1 = makeLabel("Mostrar torre", -68.f, 10.f);

        auto t2 = CCMenuItemToggler::createWithStandardSprites(this, menu_selector(PersonalizePopup::onLine), 0.7f);
        t2->toggle(ci::getShowLine());
        m_buttonMenu->addChildAtPosition(t2, Anchor::Center, {-100.f, -108.f});
        auto l2 = makeLabel("Linea del jugador", -108.f, 10.f);
        (void)l1; (void)l2;

        this->refreshLabels();
        return true;
    }

    void refreshLabels() {
        m_opLbl->setString(fmt::format("Opacidad barra verde: {}%", static_cast<int>(std::round(ci::getOpacity() * 100.f))).c_str());
        m_sizeLbl->setString(fmt::format("Tamano de la torre: {}%", static_cast<int>(std::round(ci::getTowerScale() * 100.f))).c_str());
        m_distLbl->setString(fmt::format("Aviso de la torre: {} bloques", static_cast<int>(std::round(ci::getWarnBlocks()))).c_str());
    }

    void onOpacity(CCObject* sender) {
        float v = std::clamp(static_cast<SliderThumb*>(sender)->getValue(), 0.f, 1.f);
        Mod::get()->setSavedValue<float>("green-opacity", v);
        this->refreshLabels();
        ci::refreshOverlay();
    }

    void onSize(CCObject* sender) {
        float v = std::clamp(static_cast<SliderThumb*>(sender)->getValue(), 0.f, 1.f);
        Mod::get()->setSavedValue<float>("tower-scale", 0.5f + v);
        this->refreshLabels();
        ci::refreshOverlay();
    }

    void onDist(CCObject* sender) {
        float v = std::clamp(static_cast<SliderThumb*>(sender)->getValue(), 0.f, 1.f);
        Mod::get()->setSavedValue<float>("warn-blocks", 5.f + v * 25.f);
        this->refreshLabels();
    }

    void onTower(CCObject* sender) {
        bool on = !static_cast<CCMenuItemToggler*>(sender)->isToggled();
        Mod::get()->setSavedValue<bool>("show-tower", on);
        ci::refreshOverlay();
    }

    void onLine(CCObject* sender) {
        bool on = !static_cast<CCMenuItemToggler*>(sender)->isToggled();
        Mod::get()->setSavedValue<bool>("show-player-line", on);
        ci::refreshOverlay();
    }

public:
    static PersonalizePopup* create() {
        auto ret = new PersonalizePopup();
        if (ret->init()) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }
};

// =====================================================================
//  Menu principal: Empezar/Detener, Guardar, Borrar, Usar, Guardados
// =====================================================================
class MenuPopup : public geode::Popup {
protected:
    ButtonSprite* m_startSpr = nullptr;
    ButtonSprite* m_useSpr = nullptr;
    CCLabelBMFont* m_status = nullptr;

    bool init() {
        if (!Popup::init(240.f, 340.f)) return false;
        this->setTitle("Click Indicator");

        auto makeBtn = [this](char const* text, SEL_MenuHandler cb, float y, ButtonSprite*& out) {
            out = ButtonSprite::create(text, 150, true, "bigFont.fnt", "GJ_button_01.png", 30.f, 0.7f);
            auto btn = CCMenuItemSpriteExtra::create(out, this, cb);
            m_buttonMenu->addChildAtPosition(btn, Anchor::Center, {0.f, y});
        };
        ButtonSprite* tmp = nullptr;

        makeBtn("Empezar", menu_selector(MenuPopup::onStart), 90.f, m_startSpr);
        makeBtn("Guardar", menu_selector(MenuPopup::onSave), 52.f, tmp);
        makeBtn("Borrar", menu_selector(MenuPopup::onDelete), 14.f, tmp);
        makeBtn("Usar", menu_selector(MenuPopup::onUse), -24.f, m_useSpr);

        // Boton de abajo: abre la lista de guardados
        auto listSpr = ButtonSprite::create("Guardados", 150, true, "bigFont.fnt", "GJ_button_02.png", 30.f, 0.7f);
        auto listBtn = CCMenuItemSpriteExtra::create(listSpr, this, menu_selector(MenuPopup::onList));
        m_buttonMenu->addChildAtPosition(listBtn, Anchor::Center, {0.f, -70.f});

        auto perSpr = ButtonSprite::create("Personalizar", 150, true, "bigFont.fnt", "GJ_button_02.png", 30.f, 0.7f);
        auto perBtn = CCMenuItemSpriteExtra::create(perSpr, this, menu_selector(MenuPopup::onCustomize));
        m_buttonMenu->addChildAtPosition(perBtn, Anchor::Center, {0.f, -108.f});

        m_status = CCLabelBMFont::create("", "chatFont.fnt");
        m_status->setScale(0.8f);
        m_mainLayer->addChildAtPosition(m_status, Anchor::Top, {0.f, -42.f});

        this->refreshUI();
        return true;
    }

    void refreshUI() {
        m_startSpr->setString(ci::recording ? "Detener" : "Empezar");
        m_useSpr->setString(ci::usingSaved ? "Dejar de usar" : "Usar");

        std::string st;
        if (ci::recording) st = "Grabando... (" + std::to_string(ci::current.size()) + " clicks)";
        else if (!ci::selectedName.empty()) st = "Seleccionado: " + ci::selectedName;
        else if (!ci::current.empty()) st = "Sin guardar: " + std::to_string(ci::current.size()) + " clicks";
        else st = "Nada seleccionado";
        m_status->setString(st.c_str());
    }

    void onStart(CCObject*) {
        auto pl = PlayLayer::get();
        if (!pl) return;

        if (ci::recording) {
            // DETENER
            ci::recording = false;
            ci::holding = false;
            Notification::create("Grabacion detenida", NotificationIcon::Info)->show();
        } else {
            // EMPEZAR
            if (!pl->m_isPracticeMode) {
                ci::alertMsg("Click Indicator", "Activa el <cy>modo practica</c> para grabar.");
                return;
            }
            ci::current.clear();
            ci::holding = false;
            ci::recording = true;
            Notification::create("Grabando clicks...", NotificationIcon::Success)->show();
        }
        this->refreshUI();
    }

    void onSave(CCObject*) {
        if (ci::recording) {
            ci::alertMsg("Click Indicator", "Detén la grabacion antes de guardar.");
            return;
        }
        if (ci::current.empty()) {
            ci::alertMsg("Click Indicator", "No hay clicks grabados. Pulsa <cy>Empezar</c> y juega en practica.");
            return;
        }
        NamePopup::create()->show();
    }

    void onDelete(CCObject*) {
        if (ci::selectedPath.empty()) {
            ci::alertMsg("Click Indicator", "Selecciona una grabacion en <cy>Guardados</c> primero.");
            return;
        }
        auto name = ci::selectedName;
        createQuickPopup(
            "Borrar", "Borrar la grabacion <cy>" + name + "</c>?", "No", "Si",
            [this](FLAlertLayer*, bool yes) {
                if (!yes) return;
                ci::deleteSelected();
                this->refreshUI();
            }
        );
    }

    void onUse(CCObject*) {
        if (ci::usingSaved) {
            ci::usingSaved = false;
        } else {
            if (ci::loaded.empty()) {
                ci::alertMsg("Click Indicator", "No hay nada seleccionado. Elige una grabacion en <cy>Guardados</c>.");
                return;
            }
            ci::usingSaved = true;
        }
        ci::refreshOverlay();
        this->refreshUI();
    }

    void onList(CCObject*) {
        SavedListPopup::create()->show();
    }

    void onCustomize(CCObject*) {
        PersonalizePopup::create()->show();
    }

public:
    void closeMenu() {
        this->onClose(nullptr);
    }

    static MenuPopup* create() {
        auto ret = new MenuPopup();
        if (ret->init()) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }
};

// =====================================================================
//  Boton en el menu de pausa
// =====================================================================
class $modify(CIPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        auto winSize = CCDirector::get()->getWinSize();
        auto menu = CCMenu::create();
        menu->setID("click-indicator-menu"_spr);
        menu->setPosition({0.f, 0.f});

        auto spr = ButtonSprite::create("Click Indicator", 120, true, "bigFont.fnt", "GJ_button_04.png", 28.f, 0.5f);
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(CIPauseLayer::onClickIndicator));
        btn->setPosition({winSize.width - 75.f, 25.f});
        menu->addChild(btn);

        this->addChild(menu, 10);
    }

    void onClickIndicator(CCObject*) {
        MenuPopup::create()->show();
    }
};

// =====================================================================
//  Tecla O: abre / cierra el menu (solo dentro de un nivel)
// =====================================================================
class $modify(CIKeyboardDispatcher, CCKeyboardDispatcher) {
    bool dispatchKeyboardMSG(enumKeyCodes key, bool down, bool repeat, double timestamp) {
        if (down && !repeat && key == KEY_O && PlayLayer::get()) {
            auto scene = CCScene::get();
            if (scene) {
                if (auto open = scene->getChildByType<MenuPopup>(0)) {
                    // Ya esta abierto: lo cerramos
                    open->closeMenu();
                    return true;
                }
                // Si hay otro popup abierto (por ejemplo el de nombre), no hacemos nada
                if (!scene->getChildByType<FLAlertLayer>(0)) {
                    MenuPopup::create()->show();
                    return true;
                }
            }
        }
        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, repeat, timestamp);
    }
};

// =====================================================================
//  Grabacion de clicks (solo el jugador 1)
// =====================================================================
class $modify(CIPlayerObject, PlayerObject) {
    bool pushButton(PlayerButton btn) {
        bool res = PlayerObject::pushButton(btn);
        auto pl = PlayLayer::get();
        if (pl && ci::recording && btn == PlayerButton::Jump && this == pl->m_player1) {
            if (!pl->m_isPracticeMode) {
                ci::recording = false;
                ci::holding = false;
            } else {
                ci::onPress(this->getPositionX());
            }
        }
        return res;
    }

    bool releaseButton(PlayerButton btn) {
        bool res = PlayerObject::releaseButton(btn);
        auto pl = PlayLayer::get();
        if (pl && ci::recording && btn == PlayerButton::Jump && this == pl->m_player1) {
            ci::onRelease(this->getPositionX());
        }
        return res;
    }
};

// =====================================================================
//  Ciclo de vida del nivel
// =====================================================================
class $modify(CIPlayLayer, PlayLayer) {
    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();
        ci::resetAll();
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        // Tras morir / reaparecer: descarta los clicks "del futuro" respecto al checkpoint
        if (ci::recording && m_player1) {
            ci::truncateAt(m_player1->getPositionX());
        }
        // El overlay vive en m_objectLayer; lo re-dibujamos por seguridad
        if (ci::usingSaved) ci::refreshOverlay();
    }

    void levelComplete() {
        if (ci::recording) {
            if (ci::holding && m_player1) ci::onRelease(m_player1->getPositionX());
            ci::recording = false;
            ci::holding = false;
            if (!ci::current.empty()) {
                Notification::create(
                    "Nivel completado: guarda la grabacion desde el menu de pausa",
                    NotificationIcon::Success
                )->show();
            }
        }
        PlayLayer::levelComplete();
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        if (ci::usingSaved) ci::updateVisuals(this);
    }

    void onQuit() {
        ci::usingSaved = false;
        ci::clearVisuals();
        PlayLayer::onQuit();
    }
};
