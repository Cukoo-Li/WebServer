#include "sort_timer_list.h"

#include <stdio.h>

SortTimerList::~SortTimerList() {
    Timer* tmp = head_;
    while (tmp) {
        head_ = tmp->next;
        delete tmp;
        tmp = head_;
    }
}

void SortTimerList::AddTimer(Timer* timer) {
    if (!timer)
        return;
    if (!head_) {
        head_ = timer;
        tail_ = timer;
        return;
    }
    // 考察能否插入到表头
    if (timer->expire < head_->expire) {
        timer->next = head_;
        head_->prev = timer;
        head_ = timer;
        return;
    }
    // 插入到合适的位置
    AddTimer(timer, head_);
}

// 当某个定时器发生变化时，调整它在链表的位置
// 这个函数只考虑定时器超时时间延长的情况，即该定时器需要往链表的尾部移动
void SortTimerList::AdjustTimer(Timer* timer) {
    if (!timer)
        return;
    // 如果目标定时器是尾结点，或者该定时器新的超时时间仍小于其下一个定时器，则不用调整
    if (!timer->next || timer->expire < timer->next->expire)
        return;
    // 如果目标定时器是头结点，则将其从链表中取出并重新插入链表
    if (timer == head_) {
        head_ = head_->next;
        head_->prev = nullptr;
        timer->next = nullptr;
        AddTimer(timer, head_);
    } else {
        // 如果目标定时器不是头结点，则将定时器从链表中取出，然后插入到其原来所在位置之后的部分链表中
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        AddTimer(timer, timer->next);
    }
}

void SortTimerList::DeleteTimer(Timer* timer) {
    if (!timer)
        return;
    // 如果链表中只有一个定时器，即目标定时器
    if (timer == head_ && timer == tail_) {
        delete timer;
        head_ = nullptr;
        tail_ = nullptr;
        return;
    }
    // 如果链表中至少有两个定时器，且目标定时器是链表的头结点
    if (timer == head_) {
        head_ = head_->next;
        head_->prev = nullptr;
        delete timer;
        return;
    }
    // 如果链表中至少有两个定时器，且目标定时器是链表的头结点
    if (timer == tail_) {
        tail_ = tail_->prev;
        tail_->next = nullptr;
        delete timer;
        return;
    }
    // 如果目标定时器位于链表中间
    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
}

// 处理链表上到期的任务
void SortTimerList::Tick() {
    if (!head_)
        return;
    // printf("timer tick\n");
    time_t cur_time = time(nullptr);
    Timer* tmp = head_;
    // 从头结点开始依次处理每个到期的定时器
    while (tmp) {
        if (cur_time < tmp->expire) {
            break;
        }
        // 调用定时器的回调函数，以执行定时任务
        printf("该连接超时，断开该连接\n");
        tmp->cb_func(tmp->client_data);
        // 删除定时器
        head_ = tmp->next;
        if (head_)
            head_->prev = nullptr;
        delete tmp;
        tmp = head_;
    }

}

// 一个重载的辅助函数
// 该函数表示将目标定时器timer添加到结点lst_head之后的部分链表中
void SortTimerList::AddTimer(Timer* timer, Timer* lst_head) {
    Timer* prev = lst_head;
    Timer* tmp = prev->next;
    // 寻找超时时间比目标定时器大的结点，插入到该结点之前
    while (tmp) {
        if (timer->expire < tmp->expire) {
            prev->next = timer;
            timer->next = tmp;
            tmp->prev = timer;
            timer->prev = prev;
            return;
        }
        prev = tmp;
        tmp = tmp->next;
    }
    // 没找到
    prev->next = timer;
    timer->prev = prev;
    timer->next = nullptr;
    tail_ = timer;
}