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
        if (!Popup::init(260.f, 245.f)) return false;
        this->setTitle("Guardados");

        auto folderSpr = ButtonSprite::create("Carpeta", 90, true, "bigFont.fnt", "GJ_button_04.png", 24.f, 0.5f);
        auto folderBtn = CCMenuItemSpriteExtra::create(folderSpr, this, menu_selector(SavedListPopup::onFolder));
        m_buttonMenu->addChildAtPosition(folderBtn, Anchor::Bottom, {0.f, 20.f});

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

        m_mainLayer->addChildAtPosition(scroll, Anchor::Center, {-W / 2.f, -H / 2.f + 2.f});
        return true;
    }

    void onFolder(CCObject*) {
        utils::file::openFolder(ci::recordingsDir());
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

    CCMenuItemToggler* makeToggle(char const* text, float y, bool value, SEL_MenuHandler cb) {
        auto t = CCMenuItemToggler::createWithStandardSprites(this, cb, 0.7f);
        t->toggle(value);
        m_buttonMenu->addChildAtPosition(t, Anchor::Center, {-105.f, y});
        auto lbl = CCLabelBMFont::create(text, "bigFont.fnt");
        lbl->setScale(0.4f);
        lbl->setAnchorPoint({0.f, 0.5f});
        m_mainLayer->addChildAtPosition(lbl, Anchor::Center, {-82.f, y});
        return t;
    }

    bool init() {
        if (!Popup::init(290.f, 400.f)) return false;
        this->setTitle("Personalizar");

        m_opLbl = makeLabel("", 140.f);
        makeSlider(120.f, menu_selector(PersonalizePopup::onOpacity), ci::getOpacity());

        m_sizeLbl = makeLabel("", 88.f);
        makeSlider(68.f, menu_selector(PersonalizePopup::onSize), ci::getTowerScale() - 0.5f);

        m_distLbl = makeLabel("", 36.f);
        makeSlider(16.f, menu_selector(PersonalizePopup::onDist), (ci::getWarnBlocks() - 5.f) / 25.f);

        makeToggle("Mostrar carril de cuadros", -30.f, ci::getShowTower(), menu_selector(PersonalizePopup::onTower));
        makeToggle("Linea del jugador", -70.f, ci::getShowLine(), menu_selector(PersonalizePopup::onLine));
        makeToggle("Mostrar juicios", -110.f, ci::getShowJudge(), menu_selector(PersonalizePopup::onJudge));
        makeToggle("Combo y precision", -150.f, ci::getShowStats(), menu_selector(PersonalizePopup::onStats));

        this->refreshLabels();
        return true;
    }

    void refreshLabels() {
        m_opLbl->setString(fmt::format("Opacidad de la barra verde: {}%", static_cast<int>(std::round(ci::getOpacity() * 100.f))).c_str());
        m_sizeLbl->setString(fmt::format("Tamano del carril: {}%", static_cast<int>(std::round(ci::getTowerScale() * 100.f))).c_str());
        m_distLbl->setString(fmt::format("Distancia de los cuadros: {} bloques", static_cast<int>(std::round(ci::getWarnBlocks()))).c_str());
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

    void onJudge(CCObject* sender) {
        bool on = !static_cast<CCMenuItemToggler*>(sender)->isToggled();
        Mod::get()->setSavedValue<bool>("show-judge", on);
    }

    void onStats(CCObject* sender) {
        bool on = !static_cast<CCMenuItemToggler*>(sender)->isToggled();
        Mod::get()->setSavedValue<bool>("show-stats", on);
        ci::updateStats();
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
//  Popup: Temas
// =====================================================================
class ThemesPopup : public geode::Popup {
protected:
    bool init() {
        if (!Popup::init(300.f, 300.f)) return false;
        this->setTitle("Temas");

        constexpr float rowH = 58.f;
        float y0 = (ci::THEME_COUNT - 1) * rowH / 2.f - 8.f;

        for (int i = 0; i < ci::THEME_COUNT; i++) {
            auto const& t = ci::themeAt(i);
            float y = y0 - i * rowH;
            bool selected = (i == ci::themeIndex());

            auto spr = ButtonSprite::create(
                t.name, 150, true, "bigFont.fnt",
                selected ? "GJ_button_02.png" : "GJ_button_01.png", 30.f, 0.7f
            );
            auto btn = CCMenuItemExt::createSpriteExtra(spr, [this, i](CCMenuItemSpriteExtra*) {
                Mod::get()->setSavedValue<int>("theme", i);
                ci::refreshOverlay();
                Notification::create(std::string("Tema: ") + ci::themeAt(i).name, NotificationIcon::Success)->show();
                this->onClose(nullptr);
            });
            m_buttonMenu->addChildAtPosition(btn, Anchor::Center, {0.f, y + 8.f});

            auto desc = CCLabelBMFont::create(t.desc, "chatFont.fnt");
            desc->setScale(0.62f);
            desc->setOpacity(190);
            m_mainLayer->addChildAtPosition(desc, Anchor::Center, {0.f, y - 14.f});
        }
        return true;
    }

public:
    static ThemesPopup* create() {
        auto ret = new ThemesPopup();
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
        if (!Popup::init(240.f, 385.f)) return false;
        this->setTitle("Click Indicator");

        auto makeBtn = [this](char const* text, SEL_MenuHandler cb, float y, ButtonSprite*& out, char const* bg = "GJ_button_01.png") {
            out = ButtonSprite::create(text, 150, true, "bigFont.fnt", bg, 30.f, 0.7f);
            auto btn = CCMenuItemSpriteExtra::create(out, this, cb);
            m_buttonMenu->addChildAtPosition(btn, Anchor::Center, {0.f, y});
        };
        ButtonSprite* tmp = nullptr;

        makeBtn("Empezar", menu_selector(MenuPopup::onStart), 118.f, m_startSpr);
        makeBtn("Guardar", menu_selector(MenuPopup::onSave), 80.f, tmp);
        makeBtn("Borrar", menu_selector(MenuPopup::onDelete), 42.f, tmp);
        makeBtn("Usar", menu_selector(MenuPopup::onUse), 4.f, m_useSpr);

        // Botones de abajo
        makeBtn("Guardados", menu_selector(MenuPopup::onList), -42.f, tmp, "GJ_button_02.png");
        makeBtn("Personalizar", menu_selector(MenuPopup::onCustomize), -80.f, tmp, "GJ_button_02.png");
        makeBtn("Temas", menu_selector(MenuPopup::onThemes), -118.f, tmp, "GJ_button_02.png");

        m_status = CCLabelBMFont::create("", "chatFont.fnt");
        m_status->setScale(0.8f);
        m_mainLayer->addChildAtPosition(m_status, Anchor::Top, {0.f, -40.f});

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
        ci::rememberUse();
        ci::refreshOverlay();
        if (ci::usingSaved) {
            if (auto pl = PlayLayer::get(); pl && pl->m_player1) ci::resetJudge(pl->m_player1->getPositionX());
        }
        this->refreshUI();
    }

    void onList(CCObject*) {
        SavedListPopup::create()->show();
    }

    void onCustomize(CCObject*) {
        PersonalizePopup::create()->show();
    }

    void onThemes(CCObject*) {
        ThemesPopup::create()->show();
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
        if (pl && ci::usingSaved && btn == PlayerButton::Jump && this == pl->m_player1) {
            ci::judgePress(this->getPositionX());
        }
        return res;
    }

    bool releaseButton(PlayerButton btn) {
        bool res = PlayerObject::releaseButton(btn);
        auto pl = PlayLayer::get();
        if (pl && ci::recording && btn == PlayerButton::Jump && this == pl->m_player1) {
            ci::onRelease(this->getPositionX());
        }
        if (pl && ci::usingSaved && btn == PlayerButton::Jump && this == pl->m_player1) {
            ci::judgeRelease(this->getPositionX());
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
        ci::autoLoad(this);   // vuelve a cargar la ultima grabacion usada en este nivel
        ci::refreshOverlay();
        ci::resetJudge(m_player1 ? m_player1->getPositionX() : 0.f);
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        // Tras morir / reaparecer: descarta los clicks "del futuro" respecto al checkpoint
        if (ci::recording && m_player1) {
            ci::truncateAt(m_player1->getPositionX());
        }
        // El overlay vive en m_objectLayer; lo re-dibujamos por seguridad
        if (ci::usingSaved) {
            ci::refreshOverlay();
            ci::resetJudge(m_player1 ? m_player1->getPositionX() : 0.f);
        }
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
        if (ci::usingSaved) ci::updateVisuals(this, dt);
    }

    void onQuit() {
        ci::usingSaved = false;
        ci::clearVisuals();
        PlayLayer::onQuit();
    }
};
