#include <Geode/Geode.hpp>

using namespace geode::prelude;

#include <Color.hpp>
#include <Geode/modify/CCControlColourPicker.hpp>
#include <Geode/ui/GeodeUI.hpp>
#include <TouchArea.hpp>

struct ConvInfo {
    std::function<Vec3(float, float, float)> fromRgb;
    std::function<Vec3(float, float, float)> toRgb;
    float sliderVL;
    std::string vlLabel;
};

ConvInfo currentConvInfo() {
    auto space = Mod::get()->getSettingValue<std::string>("color-space");

    if (space == "HSV") {
        return {rgb_to_hsv, hsv_to_rgb, 1.0, "V:"};
    } else if (space == "HSL") {
        return {rgb_to_hsl, hsl_to_rgb, 0.5, "L:"};
    } else if (space == "OkHSV") {
        return {srgb_to_okhsv, okhsv_to_srgb, 1.0, "V:"};
    } else if (space == "OkHSL") {
        return {srgb_to_okhsl, okhsl_to_srgb, 0.6, "L:"};
    }

    return {rgb_to_hsv, hsv_to_rgb, 1.0, "V:"};
}

ccColor3B tripleToB(Vec3 v) {
    return ccColor3B {
        (GLubyte)(std::clamp(v.x, 0.0f, 1.0f) * 255.f),
        (GLubyte)(std::clamp(v.y, 0.0f, 1.0f) * 255.f),
        (GLubyte)(std::clamp(v.z, 0.0f, 1.0f) * 255.f)
    };
}

class $modify(MyCCControlColourPicker, CCControlColourPicker) {
    struct Fields {
        CCDrawNode* squareDraw;
        CCDrawNode* sliderDraw;
        CCSprite* pickerDot;
        CCSprite* sliderHandle;
        Vec3 hsvl;
        int squareDrawThrottle;
        TextInput* hInput;
        TextInput* sInput;
        TextInput* vlInput;
        CCLabelBMFont* vlLabel;

        // EventListener<SettingChangedEventV3> m_colorSpaceSettingListener = {
        //     [this](std::shared_ptr<SettingV3> setting) { return ListenerResult::Propagate; },
        //     SettingChangedEventV3(Mod::get(), "color-space")
        // };
        // EventListener<SettingChangedEventV3> m_affectSliderSettingListener = {
        //     [this](std::shared_ptr<SettingV3> setting) { return ListenerResult::Propagate; },
        //     SettingChangedEventV3(Mod::get(), "affect-slider")
        // };

        ListenerHandle m_colorSpaceSettingListener = SettingChangedEventV3().listen([this](std::shared_ptr<SettingV3> setting) {
            return ListenerResult::Propagate;
        });

        ListenerHandle m_affectSliderSettingListener = SettingChangedEventV3().listen([this](std::shared_ptr<SettingV3> setting) {
            return ListenerResult::Propagate;
        });
    };

    virtual bool init();
    virtual void setColorValue(ccColor3B const& v);

    void hsvlChanged(bool setRgb, bool redrawSquare);
    void onOptions(CCObject* sender);
};

bool MyCCControlColourPicker::init() {
    if (!CCControlColourPicker::init()) {
        return false;
    }

    if (LevelEditorLayer::get() == nullptr) {
        return true;
    }

    getChildByType<CCSpriteBatchNode>(0)->setVisible(false);
    getChildByType<CCControlHuePicker>(0)->setVisible(false);
    getChildByType<CCControlSaturationBrightnessPicker>(0)->setVisible(false);

    m_fields->squareDrawThrottle = 0;
    // m_fields->m_colorSpaceSettingListener = {
    //     [this](std::shared_ptr<SettingV3> setting) {
    //         setColorValue(m_rgb);

    //         return ListenerResult::Propagate;
    //     },
    //     SettingChangedEventV3(Mod::get(), "color-space")
    // };

    m_fields->m_colorSpaceSettingListener = SettingChangedEventV3().listen([this](std::shared_ptr<SettingV3> setting) {
        setColorValue(m_rgb);
        return ListenerResult::Propagate;
    });

    // m_fields->m_affectSliderSettingListener = {
    //     [this](std::shared_ptr<SettingV3> setting) {
    //         setColorValue(m_rgb);

    //         return ListenerResult::Propagate;
    //     },
    //     SettingChangedEventV3(Mod::get(), "affect-slider")
    // };

    m_fields->m_affectSliderSettingListener = SettingChangedEventV3().listen([this](std::shared_ptr<SettingV3> setting) {
        setColorValue(m_rgb);
        return ListenerResult::Propagate;
    });

    m_fields->squareDraw = CCDrawNode::create();
    addChild(m_fields->squareDraw);
    m_fields->squareDraw->setPosition(-80, -45);

    m_fields->sliderDraw = CCDrawNode::create();
    addChild(m_fields->sliderDraw);
    m_fields->sliderDraw->setPosition(55, -45);

    auto squareOutline = CCSprite::create("ColorSquareOutline.png"_spr);
    addChild(squareOutline);
    squareOutline->setPosition(CCPoint(-20, 15));
    auto sliderOutline = CCSprite::create("ColorSliderOutline.png"_spr);
    addChild(sliderOutline);
    sliderOutline->setPosition(CCPoint(65, 15));

    auto pickerDotOutline = CCSprite::create("PickerDotOutline.png"_spr);
    m_fields->pickerDot = CCSprite::create("PickerDot.png"_spr);
    addChild(m_fields->pickerDot);
    m_fields->pickerDot->addChild(pickerDotOutline);
    m_fields->pickerDot->setPosition(CCPoint(-80, -45));
    m_fields->pickerDot->setZOrder(1);
    pickerDotOutline->setAnchorPoint(CCPoint(0, 0));

    auto sliderHandleOutline = CCSprite::create("SliderHandleOutline.png"_spr);
    m_fields->sliderHandle = CCSprite::create("SliderHandle.png"_spr);
    addChild(m_fields->sliderHandle);
    m_fields->sliderHandle->addChild(sliderHandleOutline);
    m_fields->sliderHandle->setPosition(CCPoint(65, -45));
    m_fields->sliderHandle->setZOrder(1);
    sliderHandleOutline->setAnchorPoint(CCPoint(0, 0));

    auto squareTouch = TouchArea::create(
        [this](auto v) {
            if (v.x >= -7.0 && v.x <= 127.0 && v.y >= -7.0 && v.y <= 127.0) {
                m_fields->hsvl.y = std::clamp(v.x, 0.f, 120.f) / 120.0;
                m_fields->hsvl.z = std::clamp(v.y, 0.f, 120.f) / 120.0;
                hsvlChanged(true, false);

                return true;
            }
            return false;
        },
        [this](auto v) {
            m_fields->hsvl.y = std::clamp(v.x, 0.f, 120.f) / 120.0;
            m_fields->hsvl.z = std::clamp(v.y, 0.f, 120.f) / 120.0;
            hsvlChanged(true, false);
        },
        [this](auto v) {}
    );
    auto sliderTouch = TouchArea::create(
        [this](auto v) {
            if (v.x >= -7.0 && v.x <= 27.0 && v.y >= -7.0 && v.y <= 127.0) {
                m_fields->hsvl.x = std::clamp(v.y, 0.f, 120.f) / 120.0;
                m_fields->squareDrawThrottle = 0;
                hsvlChanged(true, true);

                return true;
            }
            return false;
        },
        [this](auto v) {
            m_fields->hsvl.x = std::clamp(v.y, 0.f, 120.f) / 120.0;
            hsvlChanged(true, m_fields->squareDrawThrottle == 0);
            m_fields->squareDrawThrottle = (m_fields->squareDrawThrottle + 1) % 8;
        },
        [this](auto v) {
            m_fields->hsvl.x = std::clamp(v.y, 0.f, 120.f) / 120.0;
            hsvlChanged(true, true);
        }
    );

    addChild(squareTouch);
    addChild(sliderTouch);
    squareTouch->setPosition(CCPoint(-80, -45));
    sliderTouch->setPosition(CCPoint(55, -45));

    auto menu = CCMenu::create();

    auto btnSprite = CCSprite::create("ColorOptions.png"_spr);
    btnSprite->setScale(0.6);
    auto btn =
        CCMenuItemSpriteExtra::create(btnSprite, this, menu_selector(MyCCControlColourPicker::onOptions));
    btn->setPosition(65.0, -68.0);
    menu->addChild(btn);
    menu->setPosition(CCPoint(0, 0));
    menu->setTouchPriority(CCTouchDispatcher::get()->getTargetPrio() - 1);

    addChild(menu);

    m_fields->hInput = geode::TextInput::create(50.0, "");
    m_fields->hInput->setScale(0.6);
    m_fields->hInput->setPosition(-61, -75);
    m_fields->hInput->setCommonFilter(CommonFilter::Uint);
    m_fields->hInput->setCallback([this](auto _s) {
        std::string s = _s == "" ? "0" : _s;
        auto n = std::clamp(geode::utils::numFromString<uint32_t>(s).unwrap(), 0u, 360u);
        m_fields->hsvl.x = (float)n / 360.0;
        hsvlChanged(true, true);
    });
    addChild(m_fields->hInput);

    m_fields->sInput = geode::TextInput::create(50.0, "");
    m_fields->sInput->setScale(0.6);
    m_fields->sInput->setPosition(-21, -75);
    m_fields->sInput->setCommonFilter(CommonFilter::Uint);
    m_fields->sInput->setCallback([this](auto _s) {
        std::string s = _s == "" ? "0" : _s;
        auto n = std::clamp(geode::utils::numFromString<uint32_t>(s).unwrap(), 0u, 100u);
        m_fields->hsvl.y = (float)n / 100.0;
        hsvlChanged(true, false);
    });
    addChild(m_fields->sInput);

    m_fields->vlInput = geode::TextInput::create(50.0, "");
    m_fields->vlInput->setScale(0.6);
    m_fields->vlInput->setPosition(19, -75);
    m_fields->vlInput->setCommonFilter(CommonFilter::Uint);
    m_fields->vlInput->setCallback([this](auto _s) {
        std::string s = _s == "" ? "0" : _s;
        auto n = std::clamp(geode::utils::numFromString<uint32_t>(s).unwrap(), 0u, 100u);
        m_fields->hsvl.z = (float)n / 100.0;
        hsvlChanged(true, false);
    });
    addChild(m_fields->vlInput);

    auto hLabel = CCLabelBMFont::create("H:", "goldFont.fnt");
    hLabel->setScale(0.5);
    hLabel->setPosition(-61, -57);
    addChild(hLabel);
    auto sLabel = CCLabelBMFont::create("S:", "goldFont.fnt");
    sLabel->setScale(0.5);
    sLabel->setPosition(-21, -57);
    addChild(sLabel);
    m_fields->vlLabel = CCLabelBMFont::create("", "goldFont.fnt");
    m_fields->vlLabel->setScale(0.5);
    m_fields->vlLabel->setPosition(19, -57);
    addChild(m_fields->vlLabel);

    hsvlChanged(false, true);

    return true;
}

void MyCCControlColourPicker::setColorValue(ccColor3B const& v) {
    if (LevelEditorLayer::get() == nullptr) {
        return CCControlColourPicker::setColorValue(v);
    }

    m_rgb = v;
    if (m_delegate != nullptr) {
        m_delegate->colorValueChanged(m_rgb);
    }

    m_fields->hsvl = currentConvInfo().fromRgb((float)v.r / 255.f, (float)v.g / 255.f, (float)v.b / 255.f);
    if (isnan(m_fields->hsvl.x)) {
        m_fields->hsvl.x = 0.0;
    }
    if (isnan(m_fields->hsvl.y)) {
        m_fields->hsvl.y = 0.0;
    }
    if (isnan(m_fields->hsvl.z)) {
        m_fields->hsvl.z = 0.0;
    }
    hsvlChanged(false, true);
}

void MyCCControlColourPicker::hsvlChanged(bool setRgb, bool redrawSquare) {
    m_fields->hInput->setString(
        geode::utils::numToString((uint32_t)std::round(std::clamp(m_fields->hsvl.x, 0.0f, 1.0f) * 360.0))
    );
    m_fields->sInput->setString(
        geode::utils::numToString((uint32_t)std::round(std::clamp(m_fields->hsvl.y, 0.0f, 1.0f) * 100.0))
    );
    m_fields->vlInput->setString(
        geode::utils::numToString((uint32_t)std::round(std::clamp(m_fields->hsvl.z, 0.0f, 1.0f) * 100.0))
    );

    auto conv = currentConvInfo();

    m_fields->vlLabel->setString(conv.vlLabel.c_str());

    if (setRgb) {
        m_rgb = tripleToB(conv.toRgb(m_fields->hsvl.x, m_fields->hsvl.y, m_fields->hsvl.z));
        if (m_delegate != nullptr) {
            m_delegate->colorValueChanged(m_rgb);
        }
    }

    m_fields->pickerDot->setPosition(CCPoint(-80 + m_fields->hsvl.y * 120.0, -45 + m_fields->hsvl.z * 120.0));
    m_fields->pickerDot->setColor(m_rgb);

    auto sliderS = 1.0f;
    auto sliderVL = conv.sliderVL;
    if (Mod::get()->getSettingValue<bool>("affect-slider")) {
        sliderS = m_fields->hsvl.y;
        sliderVL = m_fields->hsvl.z;
    }
    m_fields->sliderHandle->setPositionY(-45 + m_fields->hsvl.x * 120.0);
    m_fields->sliderHandle->setColor(tripleToB(conv.toRgb(m_fields->hsvl.x, sliderS, sliderVL)));

    if (redrawSquare) {
        m_fields->squareDraw->clear();
        for (int y = 0; y < 120; y += 2) {
            for (int x = 0; x < 120; x += 2) {
                auto v = conv.toRgb(m_fields->hsvl.x, x / 120.f, y / 120.f);
                m_fields->squareDraw->drawRect(
                    CCPoint(x, y),
                    CCPoint(x + 2, y + 2),
                    ccColor4F {v.x, v.y, v.z, 1.0},
                    0.0,
                    ccColor4F {0.0, 0.0, 0.0, 0.0}
                );
            }
        }
    }
    // slider is cheap to draw so id rather always have it here than add another param that i could potentially mess up
    m_fields->sliderDraw->clear();
    for (int y = 0; y < 120; y += 1) {
        auto v = conv.toRgb(y / 120.f, sliderS, sliderVL);
        m_fields->sliderDraw->drawRect(
            CCPoint(0, y),
            CCPoint(20, y + 1),
            ccColor4F {v.x, v.y, v.z, 1.0},
            0.0,
            ccColor4F {0.0, 0.0, 0.0, 0.0}
        );
    }
}

void MyCCControlColourPicker::onOptions(CCObject* sender) {
    geode::openSettingsPopup(Mod::get());
}