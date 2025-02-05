#include "cont_poller.h"
#include "impl.h"

namespace NCoro {
    namespace {
        template <class T>
        int DoExecuteEvent(T* event) noexcept {
            auto* cont = event->Cont();

            if (cont->Cancelled()) {
                return ECANCELED;
            }

            cont->Executor()->ScheduleIoWait(event);
            cont->SwitchTo(cont->Executor()->SchedContext());

            if (cont->Cancelled()) {
                return ECANCELED;
            }

            return event->Status();
        }
    }

    void TContPollEvent::Wake() noexcept {
        TTreeNode::UnLink();
        TListNode::Unlink();
        Cont()->ReSchedule();
    }


    TInstant TEventWaitQueue::WakeTimedout(TInstant now) noexcept {
        TIoWait::TIterator it = IoWait_.Begin();

        if (it != IoWait_.End()) {
            if (it->DeadLine() > now) {
                return it->DeadLine();
            }

            do {
                (it++)->Wake(ETIMEDOUT);
            } while (it != IoWait_.End() && it->DeadLine() <= now);
        }

        return now;
    }

    void TEventWaitQueue::Register(NCoro::TContPollEvent* event) {
        if (event->DeadLine() == TInstant::Max()) {
            InfiniteIoWait_.PushBack(event);
        } else {
            IoWait_.Insert(event);
        }
        event->Cont()->Unlink();
    }

    void TEventWaitQueue::Abort() noexcept {
        auto canceler = [](TContPollEvent& e) {
            e.Cont()->Cancel();
        };
        auto cancelerPtr = [&](TContPollEvent* e) {
            canceler(*e);
        };
        IoWait_.ForEach(canceler);
        InfiniteIoWait_.ForEach(cancelerPtr);
    }
}

void TFdEvent::RemoveFromIOWait() noexcept {
    this->Cont()->Executor()->Poller()->Remove(this);
}

int ExecuteEvent(TFdEvent* event) noexcept {
    return NCoro::DoExecuteEvent(event);
}

int ExecuteEvent(TTimerEvent* event) noexcept {
    return NCoro::DoExecuteEvent(event);
}
