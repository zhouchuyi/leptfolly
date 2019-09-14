#include<assert.h>
#include<algorithm>
#include"Futex.h"

//let thread order the sequence of "turn"
class TurnSequncer
{
public:
    
    explicit TurnSequncer(uint32_t firstTurn=0) noexcept
        : state_(encode(firstTurn<<kTurnShift,0)) {}
    
    ~TurnSequncer() = default;

    bool isTurn(uint32_t turn) const noexcept
    {
        uint32_t state = state_.load(std::memory_order_acquire);
        return decodeCurrentSturn(state_) == (turn<<kTurnShift);
    }

    enum class TryWaitResult{ SUCCESS, PAST };

    void waitForTurn(const uint32_t turn,
                    std::atomic<uint32_t>& spinCutoff,
                    const bool updateSpinCutoff) noexcept
    {
        TryWaitResult res=TryWaitForTurn(turn,spinCutoff,updateSpinCutoff);
        assert(res==TryWaitResult::SUCCESS);
    }
///  the heart 
    TryWaitResult TryWaitForTurn(const uint32_t turn,
                                std::atomic<uint32_t>& spinCutoff,
                                const bool updateSpinCutoff) noexcept
    {
        uint32_t prevThresh = spinCutoff.load(std::memory_order_acquire);
        uint32_t effectiveSpinCutoff = updateSpinCutoff || prevThresh==0 ? kMaxSpins : prevThresh;
        uint32_t count;
        uint32_t sturn=turn<<kTurnShift;
        for( count = 0; ; count++)
        {
            uint32_t state=state_.load(std::memory_order_acquire);
            uint32_t current_strun=decodeCurrentSturn(state);
            //It is your turn without futexwait
            if(current_strun == sturn)
                break;
            
            if(sturn-current_strun >= std::numeric_limits<uint32_t>::max()/2)
                return TryWaitResult::PAST;
            
            if(count<=effectiveSpinCutoff)
            {
                // asm violate pause
                ::_mm_pause();
                continue;
            }
            uint32_t new_state;
            uint32_t current_max_waiter_delta=decodeMaxWaitersDelta(state);
            //fix me
            uint32_t my_max_waiter_delta=(sturn-current_strun)>>kTurnShift;
            if(my_max_waiter_delta <= current_max_waiter_delta)
                new_state=state;
            else
            {
                new_state=encode(current_strun,my_max_waiter_delta);
                if(state!=new_state && !state_.compare_exchange_strong(state,new_state,std::memory_order_acquire))
                    continue;
            }
            futexWait(&state_,new_state,futexChannel(turn));
            
        }
        if( updateSpinCutoff || prevThresh == 0)
        {
            uint32_t target;
            if(count >= kMaxSpins)
                target=kMaxSpins;
            else
            {
                target=std::min(uint32_t(kMaxSpins),
                                std::max(2*count,uint32_t(kMinSpins)));
            }
            
            if(prevThresh == 0)
                spinCutoff.store(target);
            else
            {
                spinCutoff.compare_exchange_weak(prevThresh,prevThresh+int(target-prevThresh) / 8);
            }
            
            
        }

        return TryWaitResult::SUCCESS;
    }
/// finish the turn and unblock the thread running waitForTurn(turn+1)
    void completeTurn(const uint32_t turn) noexcept
    {
        uint32_t state=state_.load(std::memory_order_acquire);
        while(true)
        {
            // uint32_t state=state_.load(std::memory_order_acquire);
            if(state!=encode(turn<<kTurnShift,decodeMaxWaitersDelta(state)))
                return;
            uint32_t maxWiater=decodeMaxWaitersDelta(state);
            uint32_t new_state=encode((turn+1)<<kTurnShift,
                                       maxWiater == 0 ? 0 : maxWiater-1 );
            if(state_.compare_exchange_strong(state,new_state,std::memory_order_acq_rel))
            {
                if(maxWiater!=0)
                    futexWake(&state_,std::numeric_limits<int>::max(),futexChannel(turn+1));
                break;
            }
        }
        
    }

    uint8_t uncompleteTurnLSB() const noexcept
    {
        return uint8_t(state_.load(std::memory_order_acquire)<<kTurnShift);
    }
private:
    enum : uint32_t{
        kTurnShift=6,
        kWaitersMask=(1<<kTurnShift)-1,
        kMinSpins=20,
        kMaxSpins=2000,
    };
///  26 bits record the current trun,6 bits records maxwaiters-currentturn  
    Futex state_;

    uint32_t futexChannel(uint32_t turn) const noexcept
    {
        return 1u<<(31&turn);
    }

    uint32_t decodeCurrentSturn(uint32_t state) const noexcept
    {
        return state & (~kWaitersMask);
    }

    uint32_t decodeMaxWaitersDelta(uint32_t state) const noexcept
    {
        return state & kWaitersMask;
    }

    uint32_t encode(uint32_t currentSturn,uint32_t delta)
    {
        return currentSturn | std::min(uint32_t(kWaitersMask),delta);
    }
};

