#include <TouchArea.hpp>

TouchArea* TouchArea::create(TouchAreaStartCB start_cb, TouchAreaMoveCB move_cb, TouchAreaEndCB end_cb) {
    auto ret = new TouchArea;
    if (ret->init(start_cb, move_cb, end_cb)) {
        ret->autorelease();
        return ret;
    }

    delete ret;
    return nullptr;
}

bool TouchArea::init(TouchAreaStartCB start_cb, TouchAreaMoveCB move_cb, TouchAreaEndCB end_cb) {
    setTouchMode(cocos2d::kCCTouchesOneByOne);
    setTouchEnabled(true);
    setTouchPriority(CCTouchDispatcher::get()->getTargetPrio() - 1);
    m_startCB = start_cb;
    m_moveCB = move_cb;
    m_endCB = end_cb;
    return true;
}

bool TouchArea::ccTouchBegan(CCTouch* pTouch, CCEvent* pEvent) {
    auto v = convertTouchToNodeSpace(pTouch);
    return m_startCB(v);
}

void TouchArea::ccTouchMoved(CCTouch* pTouch, CCEvent* pEvent) {
    auto v = convertTouchToNodeSpace(pTouch);
    m_moveCB(v);
}

void TouchArea::ccTouchEnded(CCTouch* pTouch, CCEvent* pEvent) {
    auto v = convertTouchToNodeSpace(pTouch);
    m_endCB(v);
}