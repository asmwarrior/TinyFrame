#ifndef TinyFrameH
#define TinyFrameH


/**
 * TinyFrame protocol library
 *
 * (c) Ondřej Hruška 2017-2018, MIT License
 * no liability/warranty, free for any use, must retain this notice & license
 *
 * Upstream URL: https://github.com/MightyPork/TinyFrame
 */


//---------------------------------------------------------------------------
#include <cstring>  // for memset()
#include <new> // placement new
//---------------------------------------------------------------------------

#include "TinyFrame_Types.hpp"
#include "TinyFrame_CRC.hpp"

namespace TinyFrame_n{

#define TF_VERSION "2.3.0"

/** TinyFrame class */

// Checksum type. Options:
//   TF_CKSUM_NONE, TF_CKSUM_XOR, TF_CKSUM_CRC8, TF_CKSUM_CRC16, TF_CKSUM_CRC32
//   TF_CKSUM_CUSTOM8, TF_CKSUM_CUSTOM16, TF_CKSUM_CUSTOM32
// Custom checksums require you to implement checksum functions (see TinyFrame.h)
template<TF_CKSUM_t TF_CKSUM_TYPE>
class TinyFrame{

    public:
    
        TinyFrame(){
            memset(&internal, 0, sizeof(internal));
        };
        ~TinyFrame() = default;

        /* --- Callback types --- */
        // This is publicly visible only to allow static init.
        /**
         * TinyFrame Type Listener callback
         *
         * @param tf - instance
         * @param msg - the received message, userdata is populated inside the object
         * @return listener result
         */
        typedef TF_Result (*TF_Listener)(TinyFrame<TF_CKSUM_TYPE> *tf, TF_Msg *msg);

        /**
         * TinyFrame Type Listener callback
         *
         * @param tf - instance
         * @param msg - the received message, userdata is populated inside the object
         * @return listener result
         */
        typedef TF_Result (*TF_Listener_Timeout)(TinyFrame<TF_CKSUM_TYPE> *tf);

        struct TF_IdListener_ {
            TF_ID id;
            TF_Listener fn;
            TF_Listener_Timeout fn_timeout;
            TF_TICKS timeout;     // nr of ticks remaining to disable this listener
            TF_TICKS timeout_max; // the original timeout is stored here (0 = no timeout)
            void *userdata;
            void *userdata2;
        };

        struct TF_TypeListener_ {
            TF_TYPE type;
            TF_Listener fn;
        };

        struct TF_GenericListener_ {
            TF_Listener fn;
        };

        struct{
            /* Public user data */
            void *userdata;
            uint32_t usertag;

            // --- the rest of the struct is internal, do not access directly ---

            /* Own state */
            TF_Peer peer_bit;       //!< Own peer bit (unqiue to avoid msg ID clash)
            TF_ID next_id;          //!< Next frame / frame chain ID

            /* Parser state */
            TF_State_ state;
            TF_TICKS parser_timeout_ticks;
            TF_ID id;               //!< Incoming packet ID
            TF_LEN len;             //!< Payload length
            uint8_t data[TF_MAX_PAYLOAD_RX]; //!< Data byte buffer
            TF_LEN rxi;             //!< Field size byte counter
            TF_CKSUM<TF_CKSUM_TYPE> cksum;         //!< Checksum calculated of the data stream
            TF_CKSUM<TF_CKSUM_TYPE> ref_cksum;     //!< Reference checksum read from the message
            TF_TYPE type;           //!< Collected message type number
            bool discard_data;      //!< Set if (len > TF_MAX_PAYLOAD) to read the frame, but ignore the data.

            /* Tx state */
            // Buffer for building frames
            uint8_t sendbuf[TF_SENDBUF_LEN]; //!< Transmit temporary buffer

            uint32_t tx_pos;        //!< Next write position in the Tx buffer (used for multipart)
            uint32_t tx_len;        //!< Total expected Tx length
            TF_CKSUM<TF_CKSUM_TYPE> tx_cksum;      //!< Transmit checksum accumulator

        #if !TF_USE_MUTEX
            bool soft_lock;         //!< Tx lock flag used if the mutex feature is not enabled.
        #endif
                
            // Those counters are used to optimize look-up times.
            // They point to the highest used slot number,
            // or close to it, depending on the removal order.
            TF_COUNT count_id_lst;
            TF_COUNT count_type_lst;
            TF_COUNT count_generic_lst;
            

            /* Transaction callbacks */
            TF_IdListener_ id_listeners[TF_MAX_ID_LST];
            TF_TypeListener_ type_listeners[TF_MAX_TYPE_LST];
            TF_GenericListener_ generic_listeners[TF_MAX_GEN_LST];

        }internal;

        //---------------------------------------------------------------------------

        /**
         * Clear message struct
         *
         * @param msg - message to clear in-place
         */
        static inline void TF_ClearMsg(TF_Msg *msg)
        {
            memset(msg, 0, sizeof(TF_Msg));
        }

        // ---------------------------------- API CALLS --------------------------------------

        /**
         * Accept incoming bytes & parse frames
         *
         * @param tf - instance
         * @param buffer - byte buffer to process
         * @param count - nr of bytes in the buffer
         */
        void TF_Accept(TinyFrame *tf, const uint8_t *buffer, uint32_t count);

        /**
         * Accept a single incoming byte
         *
         * @param tf - instance
         * @param c - a received char
         */
        void TF_AcceptChar(TinyFrame *tf, uint8_t c);

        /**
         * This function should be called periodically.
         * The time base is used to time-out partial frames in the parser and
         * automatically reset it.
         * It's also used to expire ID listeners if a timeout is set when registering them.
         *
         * A common place to call this from is the SysTick handler.
         *
         * @param tf - instance
         */
        void TF_Tick();

        /**
         * Reset the frame parser state machine.
         * This does not affect registered listeners.
         *
         * @param tf - instance
         */
        void TF_ResetParser(TinyFrame *tf);


        // ---------------------------- MESSAGE LISTENERS -------------------------------

        /**
         * Register a frame type listener.
         *
         * @param tf - instance
         * @param msg - message (contains frame_id and userdata)
         * @param cb - callback
         * @param ftimeout - time out callback
         * @param timeout - timeout in ticks to auto-remove the listener (0 = keep forever)
         * @return slot index (for removing), or TF_ERROR (-1)
         */
        bool TF_AddIdListener(TinyFrame *tf, TF_Msg *msg, TF_Listener cb, TF_Listener_Timeout ftimeout, TF_TICKS timeout);

        /**
         * Remove a listener by the message ID it's registered for
         *
         * @param tf - instance
         * @param frame_id - the frame we're listening for
         */
        bool TF_RemoveIdListener(TinyFrame *tf, TF_ID frame_id);

        /**
         * Register a frame type listener.
         *
         * @param tf - instance
         * @param frame_type - frame type to listen for
         * @param cb - callback
         * @return slot index (for removing), or TF_ERROR (-1)
         */
        bool TF_AddTypeListener(TinyFrame *tf, TF_TYPE frame_type, TF_Listener cb);

        /**
         * Remove a listener by type.
         *
         * @param tf - instance
         * @param type - the type it's registered for
         */
        bool TF_RemoveTypeListener(TinyFrame *tf, TF_TYPE type);

        /**
         * Register a generic listener.
         *
         * @param tf - instance
         * @param cb - callback
         * @return slot index (for removing), or TF_ERROR (-1)
         */
        bool TF_AddGenericListener(TinyFrame *tf, TF_Listener cb);

        /**
         * Remove a generic listener by function pointer
         *
         * @param tf - instance
         * @param cb - callback function to remove
         */
        bool TF_RemoveGenericListener(TinyFrame *tf, TF_Listener cb);

        /**
         * Renew an ID listener timeout externally (as opposed to by returning TF_RENEW from the ID listener)
         *
         * @param tf - instance
         * @param id - listener ID to renew
         * @return true if listener was found and renewed
         */
        bool TF_RenewIdListener(TinyFrame *tf, TF_ID id);


        // ---------------------------- FRAME TX FUNCTIONS ------------------------------

        /**
         * Send a frame, no listener
         *
         * @param tf - instance
         * @param msg - message struct. ID is stored in the frame_id field
         * @return success
         */
        bool TF_Send(TinyFrame *tf, TF_Msg *msg);

        /**
         * Like TF_Send, but without the struct
         */
        bool TF_SendSimple(TinyFrame *tf, TF_TYPE type, const uint8_t *data, TF_LEN len);

        /**
         * Send a frame, and optionally attach an ID listener.
         *
         * @param tf - instance
         * @param msg - message struct. ID is stored in the frame_id field
         * @param listener - listener waiting for the response (can be nullptr)
         * @param ftimeout - time out callback
         * @param timeout - listener expiry time in ticks
         * @return success
         */
        bool TF_Query(TinyFrame *tf, TF_Msg *msg, TF_Listener listener,
                    TF_Listener_Timeout ftimeout, TF_TICKS timeout);

        /**
         * Like TF_Query(), but without the struct
         */
        bool TF_QuerySimple(TinyFrame *tf, TF_TYPE type,
                            const uint8_t *data, TF_LEN len,
                            TF_Listener listener, TF_Listener_Timeout ftimeout, TF_TICKS timeout);

        /**
         * Send a response to a received message.
         *
         * @param tf - instance
         * @param msg - message struct. ID is read from frame_id. set ->renew to reset listener timeout
         * @return success
         */
        bool TF_Respond(TinyFrame *tf, TF_Msg *msg);


        // ------------------------ MULTIPART FRAME TX FUNCTIONS -----------------------------
        // Those routines are used to send long frames without having all the data available
        // at once (e.g. capturing it from a peripheral or reading from a large memory buffer)

        /**
         * TF_Send() with multipart payload.
         * msg.data is ignored and set to nullptr
         */
        bool TF_Send_Multipart(TinyFrame *tf, TF_Msg *msg);

        /**
         * TF_SendSimple() with multipart payload.
         */
        bool TF_SendSimple_Multipart(TinyFrame *tf, TF_TYPE type, TF_LEN len);

        /**
         * TF_QuerySimple() with multipart payload.
         */
        bool TF_QuerySimple_Multipart(TinyFrame *tf, TF_TYPE type, TF_LEN len, TF_Listener listener, TF_Listener_Timeout ftimeout, TF_TICKS timeout);

        /**
         * TF_Query() with multipart payload.
         * msg.data is ignored and set to nullptr
         */
        bool TF_Query_Multipart(TinyFrame *tf, TF_Msg *msg, TF_Listener listener, TF_Listener_Timeout ftimeout, TF_TICKS timeout);

        /**
         * TF_Respond() with multipart payload.
         * msg.data is ignored and set to nullptr
         */
        void TF_Respond_Multipart(TinyFrame *tf, TF_Msg *msg);

        /**
         * Send the payload for a started multipart frame. This can be called multiple times
         * if needed, until the full length is transmitted.
         *
         * @param tf - instance
         * @param buff - buffer to send bytes from
         * @param length - number of bytes to send
         */
        void TF_Multipart_Payload(TinyFrame *tf, const uint8_t *buff, uint32_t length);

        /**
         * Close the multipart message, generating chekcsum and releasing the Tx lock.
         *
         * @param tf - instance
         */
        void TF_Multipart_Close(TinyFrame *tf);


    private: 

        void _TF_FN TF_HandleReceivedMessage(TinyFrame *tf);
        uint32_t _TF_FN TF_ComposeHead(TinyFrame *tf, uint8_t *outbuff, TF_Msg *msg);
        uint32_t _TF_FN TF_ComposeBody(uint8_t *outbuff,
                                            const uint8_t *data, TF_LEN data_len,
                                            TF_CKSUM<TF_CKSUM_TYPE> *cksum);
        uint32_t _TF_FN TF_ComposeTail(uint8_t *outbuff, TF_CKSUM<TF_CKSUM_TYPE> *cksum);
        bool _TF_FN TF_SendFrame_Begin(TinyFrame *tf, TF_Msg *msg, TF_Listener listener, TF_Listener_Timeout ftimeout, TF_TICKS timeout);
        void _TF_FN TF_SendFrame_Chunk(TinyFrame *tf, const uint8_t *buff, uint32_t length);
        void _TF_FN TF_SendFrame_End(TinyFrame *tf);
        bool _TF_FN TF_SendFrame(TinyFrame *tf, TF_Msg *msg, TF_Listener listener, TF_Listener_Timeout ftimeout, TF_TICKS timeout);

        static void _TF_FN renew_id_listener(TF_IdListener_ *lst);
        void _TF_FN cleanup_id_listener(TinyFrame *tf, TF_COUNT i, TF_IdListener_ *lst);
        void _TF_FN cleanup_type_listener(TinyFrame *tf, TF_COUNT i, TF_TypeListener_ *lst);
        void _TF_FN cleanup_generic_listener(TinyFrame *tf, TF_COUNT i, TF_GenericListener_ *lst);

        void _TF_FN pars_begin_frame(TinyFrame *tf);

        #if !TF_USE_MUTEX
        bool TF_ClaimTx(TinyFrame *tf)
        void TF_ReleaseTx(TinyFrame *tf);
        #endif

}; // class TinyFrame

// ---------------------------------- INIT ------------------------------

/**
 * Initialize the TinyFrame engine.
 * This can also be used to completely reset it (removing all listeners etc).
 *
 * The field .userdata (or .usertag) can be used to identify different instances
 * in the TF_WriteImpl() function etc. Set this field after the init.
 *
 * This function is a wrapper around TF_InitStatic that calls malloc() to obtain
 * the instance.
 *
 * @param tf - instance
 * @param peer_bit - peer bit to use for self
 * @return TF instance or nullptr
 */
template <TF_CKSUM_t TF_CKSUM_TYPE>
TinyFrame<TF_CKSUM_TYPE> *TF_Init(TF_Peer peer_bit);


/**
 * Initialize the TinyFrame engine using a statically allocated instance struct.
 *
 * The .userdata / .usertag field is preserved when TF_InitStatic is called.
 *
 * @param tf - instance
 * @param peer_bit - peer bit to use for self
 * @return success
 */
template <TF_CKSUM_t TF_CKSUM_TYPE>
bool TF_InitStatic(TinyFrame<TF_CKSUM_TYPE> *tf, TF_Peer peer_bit);

/**
 * De-init the dynamically allocated TF instance
 *
 * @param tf - instance
 */
template <TF_CKSUM_t TF_CKSUM_TYPE>
void TF_DeInit(TinyFrame<TF_CKSUM_TYPE> *tf);



/**
 * Frame parser internal state.
 */

// ------------------------ TO BE IMPLEMENTED BY USER ------------------------

/**
 * 'Write bytes' function that sends data to UART
 *
 * ! Implement this in your application code !
 */
template <TF_CKSUM_t TF_CKSUM_TYPE>
extern void TF_WriteImpl(TinyFrame<TF_CKSUM_TYPE> *tf, const uint8_t *buff, uint32_t len);

// Mutex functions
#if TF_USE_MUTEX

/** Claim the TX interface before composing and sending a frame */
template <TF_CKSUM_t TF_CKSUM_TYPE>
extern bool TF_ClaimTx(TinyFrame<TF_CKSUM_TYPE> *tf);

/** Free the TX interface after composing and sending a frame */
template <TF_CKSUM_t TF_CKSUM_TYPE>
extern void TF_ReleaseTx(TinyFrame<TF_CKSUM_TYPE> *tf);

#endif

// Helper macros
#define TF_MIN(a, b) ((a)<(b)?(a):(b))
#define TF_TRY(func) do { if(!(func)) return false; } while (0)


// Type-dependent masks for bit manipulation in the ID field
#define TF_ID_MASK (TF_ID)(((TF_ID)1 << (sizeof(TF_ID)*8 - 1)) - 1)
#define TF_ID_PEERBIT (TF_ID)((TF_ID)1 << ((sizeof(TF_ID)*8) - 1))


#if !TF_USE_MUTEX
    // Not thread safe lock implementation, used if user did not provide a better one.
    // This is less reliable than a real mutex, but will catch most bugs caused by
    // inappropriate use fo the API.

    /** Claim the TX interface before composing and sending a frame */
    bool TinyFrame<TF_CKSUM_TYPE>::TF_ClaimTx(TinyFrame *tf) {
        if (tf->internal.soft_lock) {
            TF_Error("TF already locked for tx!");
            return false;
        }

        tf->internal.soft_lock = true;
        return true;
    }

    /** Free the TX interface after composing and sending a frame */
    void TinyFrame<TF_CKSUM_TYPE>::TF_ReleaseTx(TinyFrame *tf)
    {
        tf->internal.soft_lock = false;
    }
#endif


//region Init

/** Init with a user-allocated buffer */
template<TF_CKSUM_t TF_CKSUM_TYPE>
bool _TF_FN TF_InitStatic(TinyFrame<TF_CKSUM_TYPE> *tf, TF_Peer peer_bit)
{
    if (tf == nullptr) {
        TF_Error("TF_InitStatic() failed, tf is null.");
        return false;
    }

    // Zero it out, keeping user config
    uint32_t usertag = tf->internal.usertag;
    void * userdata = tf->internal.userdata;

    TinyFrame<TF_CKSUM_TYPE>* ptr = new((void*)tf) TinyFrame<TF_CKSUM_TYPE>; // placement new

    tf->internal.usertag = usertag;
    tf->internal.userdata = userdata;

    tf->internal.peer_bit = peer_bit;
    return ptr == tf; // should always be valid
}

/** Init with malloc */
template<TF_CKSUM_t TF_CKSUM_TYPE>
TinyFrame<TF_CKSUM_TYPE> * _TF_FN TF_Init(TF_Peer peer_bit)
{
    TinyFrame<TF_CKSUM_TYPE> *tf = new TinyFrame<TF_CKSUM_TYPE>();
    if (!tf) {
        TF_Error("TF_Init() failed, out of memory.");
        return nullptr;
    }

    TF_InitStatic(tf, peer_bit);
    return tf;
}

/** Release the struct */
template<TF_CKSUM_t TF_CKSUM_TYPE>
void TF_DeInit(TinyFrame<TF_CKSUM_TYPE> *tf)
{
    if (tf == nullptr) return;
    delete(tf);
}

//endregion Init


//region Listeners

/** Reset ID listener's timeout to the original value */
template<TF_CKSUM_t TF_CKSUM_TYPE>
void _TF_FN TinyFrame<TF_CKSUM_TYPE>::renew_id_listener(TF_IdListener_ *lst)
{
    lst->timeout = lst->timeout_max;
}

/** Notify callback about ID listener's demise & let it free any resources in userdata */
template<TF_CKSUM_t TF_CKSUM_TYPE>
void _TF_FN TinyFrame<TF_CKSUM_TYPE>::cleanup_id_listener(TinyFrame *tf, TF_COUNT i, TF_IdListener_ *lst)
{
    TF_Msg msg;
    if (lst->fn == nullptr) return;

    // Make user clean up their data - only if not nullptr
    if (lst->userdata != nullptr || lst->userdata2 != nullptr) {
        msg.userdata = lst->userdata;
        msg.userdata2 = lst->userdata2;
        msg.data = nullptr; // this is a signal that the listener should clean up
        lst->fn(tf, &msg); // return value is ignored here - use TF_STAY or TF_CLOSE
    }

    lst->fn = nullptr; // Discard listener
    lst->fn_timeout = nullptr;

    if (i == tf->internal.count_id_lst - 1) {
        tf->internal.count_id_lst--;
    }
}

/** Clean up Type listener */
template<TF_CKSUM_t TF_CKSUM_TYPE>
void _TF_FN TinyFrame<TF_CKSUM_TYPE>::cleanup_type_listener(TinyFrame *tf, TF_COUNT i, TF_TypeListener_ *lst)
{
    lst->fn = nullptr; // Discard listener
    if (i == tf->internal.count_type_lst - 1) {
        tf->internal.count_type_lst--;
    }
}

/** Clean up Generic listener */
template<TF_CKSUM_t TF_CKSUM_TYPE>
void _TF_FN TinyFrame<TF_CKSUM_TYPE>::cleanup_generic_listener(TinyFrame *tf, TF_COUNT i, TF_GenericListener_ *lst)
{
    lst->fn = nullptr; // Discard listener
    if (i == tf->internal.count_generic_lst - 1) {
        tf->internal.count_generic_lst--;
    }
}

/** Add a new ID listener. Returns 1 on success. */
template<TF_CKSUM_t TF_CKSUM_TYPE>
bool _TF_FN TinyFrame<TF_CKSUM_TYPE>::TF_AddIdListener(TinyFrame *tf, TF_Msg *msg, TF_Listener cb, TF_Listener_Timeout ftimeout, TF_TICKS timeout)
{
    TF_COUNT i;
    TF_IdListener_ *lst;
    for (i = 0; i < TF_MAX_ID_LST; i++) {
        lst = &tf->internal.id_listeners[i];
        // test for empty slot
        if (lst->fn == nullptr) {
            lst->fn = cb;
            lst->fn_timeout = ftimeout;
            lst->id = msg->frame_id;
            lst->userdata = msg->userdata;
            lst->userdata2 = msg->userdata2;
            lst->timeout_max = lst->timeout = timeout;
            if (i >= tf->internal.count_id_lst) {
                tf->internal.count_id_lst = (TF_COUNT) (i + 1);
            }
            return true;
        }
    }

    TF_Error("Failed to add ID listener");
    return false;
}

/** Add a new Type listener. Returns 1 on success. */
template<TF_CKSUM_t TF_CKSUM_TYPE>
bool _TF_FN TinyFrame<TF_CKSUM_TYPE>::TF_AddTypeListener(TinyFrame *tf, TF_TYPE frame_type, TF_Listener cb)
{
    TF_COUNT i;
    TF_TypeListener_ *lst;
    for (i = 0; i < TF_MAX_TYPE_LST; i++) {
        lst = &tf->internal.type_listeners[i];
        // test for empty slot
        if (lst->fn == nullptr) {
            lst->fn = cb;
            lst->type = frame_type;
            if (i >= tf->internal.count_type_lst) {
                tf->internal.count_type_lst = (TF_COUNT) (i + 1);
            }
            return true;
        }
    }

    TF_Error("Failed to add type listener");
    return false;
}

/** Add a new Generic listener. Returns 1 on success. */
template<TF_CKSUM_t TF_CKSUM_TYPE>
bool _TF_FN TinyFrame<TF_CKSUM_TYPE>::TF_AddGenericListener(TinyFrame *tf, TF_Listener cb)
{
    TF_COUNT i;
    TF_GenericListener_ *lst;
    for (i = 0; i < TF_MAX_GEN_LST; i++) {
        lst = &tf->internal.generic_listeners[i];
        // test for empty slot
        if (lst->fn == nullptr) {
            lst->fn = cb;
            if (i >= tf->internal.count_generic_lst) {
                tf->internal.count_generic_lst = (TF_COUNT) (i + 1);
            }
            return true;
        }
    }

    TF_Error("Failed to add generic listener");
    return false;
}

/** Remove a ID listener by its frame ID. Returns 1 on success. */
template<TF_CKSUM_t TF_CKSUM_TYPE>
bool _TF_FN TinyFrame<TF_CKSUM_TYPE>::TF_RemoveIdListener(TinyFrame *tf, TF_ID frame_id)
{
    TF_COUNT i;
    TF_IdListener_ *lst;
    for (i = 0; i < tf->internal.count_id_lst; i++) {
        lst = &tf->internal.id_listeners[i];
        // test if live & matching
        if (lst->fn != nullptr && lst->id == frame_id) {
            cleanup_id_listener(tf, i, lst);
            return true;
        }
    }

    TF_Error("ID listener %d to remove not found", (int)frame_id);
    return false;
}

/** Remove a type listener by its type. Returns 1 on success. */
template<TF_CKSUM_t TF_CKSUM_TYPE>
bool _TF_FN TinyFrame<TF_CKSUM_TYPE>::TF_RemoveTypeListener(TinyFrame *tf, TF_TYPE type)
{
    TF_COUNT i;
    TF_TypeListener_ *lst;
    for (i = 0; i < tf->internal.count_type_lst; i++) {
        lst = &tf->internal.type_listeners[i];
        // test if live & matching
        if (lst->fn != nullptr    && lst->type == type) {
            cleanup_type_listener(tf, i, lst);
            return true;
        }
    }

    TF_Error("Type listener %d to remove not found", (int)type);
    return false;
}

/** Remove a generic listener by its function pointer. Returns 1 on success. */
template<TF_CKSUM_t TF_CKSUM_TYPE>
bool _TF_FN TinyFrame<TF_CKSUM_TYPE>::TF_RemoveGenericListener(TinyFrame *tf, TF_Listener cb)
{
    TF_COUNT i;
    TF_GenericListener_ *lst;
    for (i = 0; i < tf->internal.count_generic_lst; i++) {
        lst = &tf->internal.generic_listeners[i];
        // test if live & matching
        if (lst->fn == cb) {
            cleanup_generic_listener(tf, i, lst);
            return true;
        }
    }

    TF_Error("Generic listener to remove not found");
    return false;
}

/** Handle a message that was just collected & verified by the parser */
template<TF_CKSUM_t TF_CKSUM_TYPE>
void _TF_FN TinyFrame<TF_CKSUM_TYPE>::TF_HandleReceivedMessage(TinyFrame *tf)
{
    TF_COUNT i;
    TF_IdListener_ *ilst;
    TF_TypeListener_ *tlst;
    TF_GenericListener_ *glst;
    TF_Result res;

    // Prepare message object
    TF_Msg msg;
    TF_ClearMsg(&msg);
    msg.frame_id = tf->internal.id;
    msg.is_response = false;
    msg.type = tf->internal.type;
    msg.data = tf->internal.data;
    msg.len = tf->internal.len;

    // Any listener can consume the message, or let someone else handle it.

    // The loop upper bounds are the highest currently used slot index
    // (or close to it, depending on the order of listener removals).

    // ID listeners first
    for (i = 0; i < tf->internal.count_id_lst; i++) {
        ilst = &tf->internal.id_listeners[i];

        if (ilst->fn && ilst->id == msg.frame_id) {
            msg.userdata = ilst->userdata; // pass userdata pointer to the callback
            msg.userdata2 = ilst->userdata2;
            res = ilst->fn(tf, &msg);
            ilst->userdata = msg.userdata; // put it back (may have changed the pointer or set to nullptr)
            ilst->userdata2 = msg.userdata2; // put it back (may have changed the pointer or set to nullptr)

            if (res != TF_NEXT) {
                // if it's TF_CLOSE, we assume user already cleaned up userdata
                if (res == TF_RENEW) {
                    renew_id_listener(ilst);
                }
                else if (res == TF_CLOSE) {
                    // Set userdata to nullptr to avoid calling user for cleanup
                    ilst->userdata = nullptr;
                    ilst->userdata2 = nullptr;
                    cleanup_id_listener(tf, i, ilst);
                }
                return;
            }
        }
    }
    // clean up for the following listeners that don't use userdata (this avoids data from
    // an ID listener that returned TF_NEXT from leaking into Type and Generic listeners)
    msg.userdata = nullptr;
    msg.userdata2 = nullptr;

    // Type listeners
    for (i = 0; i < tf->internal.count_type_lst; i++) {
        tlst = &tf->internal.type_listeners[i];

        if (tlst->fn && tlst->type == msg.type) {
            res = tlst->fn(tf, &msg);

            if (res != TF_NEXT) {
                // type listeners don't have userdata.
                // TF_RENEW doesn't make sense here because type listeners don't expire = same as TF_STAY

                if (res == TF_CLOSE) {
                    cleanup_type_listener(tf, i, tlst);
                }
                return;
            }
        }
    }

    // Generic listeners
    for (i = 0; i < tf->internal.count_generic_lst; i++) {
        glst = &tf->internal.generic_listeners[i];

        if (glst->fn) {
            res = glst->fn(tf, &msg);

            if (res != TF_NEXT) {
                // generic listeners don't have userdata.
                // TF_RENEW doesn't make sense here because generic listeners don't expire = same as TF_STAY

                // note: It's not expected that user will have multiple generic listeners, or
                // ever actually remove them. They're most useful as default callbacks if no other listener
                // handled the message.

                if (res == TF_CLOSE) {
                    cleanup_generic_listener(tf, i, glst);
                }
                return;
            }
        }
    }

    TF_Error("Unhandled message, type %d", (int)msg.type);
}

/** Externally renew an ID listener */
template<TF_CKSUM_t TF_CKSUM_TYPE>
bool _TF_FN TinyFrame<TF_CKSUM_TYPE>::TF_RenewIdListener(TinyFrame *tf, TF_ID id)
{
    TF_COUNT i;
    TF_IdListener_ *lst;
    for (i = 0; i < tf->internal.count_id_lst; i++) {
        lst = &tf->internal.id_listeners[i];
        // test if live & matching
        if (lst->fn != nullptr && lst->id == id) {
            renew_id_listener(lst);
            return true;
        }
    }

    TF_Error("Renew listener: not found (id %d)", (int)id);
    return false;
}

//endregion Listeners


//region Parser

/** Handle a received byte buffer */
template<TF_CKSUM_t TF_CKSUM_TYPE>
void _TF_FN TinyFrame<TF_CKSUM_TYPE>::TF_Accept(TinyFrame *tf, const uint8_t *buffer, uint32_t count)
{
    uint32_t i;
    for (i = 0; i < count; i++) {
        TF_AcceptChar(tf, buffer[i]);
    }
}

/** Reset the parser's internal state. */
template<TF_CKSUM_t TF_CKSUM_TYPE>
void _TF_FN TinyFrame<TF_CKSUM_TYPE>::TF_ResetParser(TinyFrame *tf)
{
    tf->internal.state = TFState_SOF;
    // more init will be done by the parser when the first byte is received
}

/** SOF was received - prepare for the frame */
template<TF_CKSUM_t TF_CKSUM_TYPE>
void _TF_FN TinyFrame<TF_CKSUM_TYPE>::pars_begin_frame(TinyFrame *tf) {
    // Reset state vars
    tf->internal.cksum = TF_CksumStart<TF_CKSUM_TYPE>(); // CKSUM_RESET
#if TF_USE_SOF_BYTE
    tf->internal.cksum = TF_CksumAdd<TF_CKSUM_TYPE>(tf->internal.cksum, TF_SOF_BYTE); // CKSUM_ADD
#endif

    tf->internal.discard_data = false;

    // Enter ID state
    tf->internal.state = TFState_ID;
    tf->internal.rxi = 0;
}

/** Handle a received char - here's the main state machine */
template<TF_CKSUM_t TF_CKSUM_TYPE>
void _TF_FN TinyFrame<TF_CKSUM_TYPE>::TF_AcceptChar(TinyFrame *tf, unsigned char c)
{
    // Parser timeout - clear
    if (tf->internal.parser_timeout_ticks >= TF_PARSER_TIMEOUT_TICKS) {
        if (tf->internal.state != TFState_SOF) {
            TF_ResetParser(tf);
            TF_Error("Parser timeout");
        }
    }
    tf->internal.parser_timeout_ticks = 0;

// DRY snippet - collect multi-byte number from the input stream, byte by byte
// This is a little dirty, but makes the code easier to read. It's used like e.g. if(),
// the body is run only after the entire number (of data type 'type') was received
// and stored to 'dest'
#define COLLECT_NUMBER(dest, type) dest = (type)(((dest) << 8) | c); \
                                   if (++tf->internal.rxi == sizeof(type))

#if !TF_USE_SOF_BYTE
    if (tf->internal.state == TFState_SOF) {
        pars_begin_frame(tf);
    }
#endif

    //@formatter:off
    switch (tf->internal.state) {
        case TFState_SOF:
            if (c == TF_SOF_BYTE) {
                pars_begin_frame(tf);
            }
            break;

        case TFState_ID:
            tf->internal.cksum = TF_CksumAdd<TF_CKSUM_TYPE>(tf->internal.cksum, c); // CKSUM_ADD
            COLLECT_NUMBER(tf->internal.id, TF_ID) {
                // Enter LEN state
                tf->internal.state = TFState_LEN;
                tf->internal.rxi = 0;
            }
            break;

        case TFState_LEN:
            tf->internal.cksum = TF_CksumAdd<TF_CKSUM_TYPE>(tf->internal.cksum, c); // CKSUM_ADD
            COLLECT_NUMBER(tf->internal.len, TF_LEN) {
                // Enter TYPE state
                tf->internal.state = TFState_TYPE;
                tf->internal.rxi = 0;
            }
            break;

        case TFState_TYPE:
            tf->internal.cksum = TF_CksumAdd<TF_CKSUM_TYPE>(tf->internal.cksum,  c); // CKSUM_ADD
            COLLECT_NUMBER(tf->internal.type, TF_TYPE) {
                #if TF_CKSUM_TYPE == TF_CKSUM_NONE
                    tf->internal.state = TFState_DATA;
                    tf->internal.rxi = 0;
                #else
                    // enter HEAD_CKSUM state
                    tf->internal.state = TFState_HEAD_CKSUM;
                    tf->internal.rxi = 0;
                    tf->internal.ref_cksum = 0;
                #endif
            }
            break;

        case TFState_HEAD_CKSUM:
            COLLECT_NUMBER(tf->internal.ref_cksum, TF_CKSUM<TF_CKSUM_TYPE>) {
                // Check the header checksum against the computed value
                tf->internal.cksum = TF_CksumEnd<TF_CKSUM_TYPE>(tf->internal.cksum);

                if (tf->internal.cksum != tf->internal.ref_cksum) {
                    TF_Error("Rx head cksum mismatch");
                    TF_ResetParser(tf);
                    break;
                }

                if (tf->internal.len == 0) {
                    // if the message has no body, we're done.
                    TF_HandleReceivedMessage(tf);
                    TF_ResetParser(tf);
                    break;
                }

                // Enter DATA state
                tf->internal.state = TFState_DATA;
                tf->internal.rxi = 0;

                tf->internal.cksum = TF_CksumStart<TF_CKSUM_TYPE>(); // CKSUM_RESET

                //Start collecting the payload
                if (tf->internal.len > TF_MAX_PAYLOAD_RX) {
                    TF_Error("Rx payload too long: %d", (int)tf->internal.len);
                    // ERROR - frame too long. Consume, but do not store.
                    tf->internal.discard_data = true;
                }
            }
            break;

        case TFState_DATA:
            if (tf->internal.discard_data) {
                tf->internal.rxi++;
            } else {
                tf->internal.cksum = TF_CksumAdd<TF_CKSUM_TYPE>(tf->internal.cksum, c); // CKSUM_ADD
                tf->internal.data[tf->internal.rxi++] = c;
            }

            if (tf->internal.rxi == tf->internal.len) {
                #if TF_CKSUM_TYPE == TF_CKSUM_NONE
                    // All done
                    TF_HandleReceivedMessage(tf);
                    TF_ResetParser(tf);
                #else
                    // Enter DATA_CKSUM state
                    tf->internal.state = TFState_DATA_CKSUM;
                    tf->internal.rxi = 0;
                    tf->internal.ref_cksum = 0;
                #endif
            }
            break;

        case TFState_DATA_CKSUM:
            COLLECT_NUMBER(tf->internal.ref_cksum, TF_CKSUM<TF_CKSUM_TYPE>) {
                // Check the header checksum against the computed value
                tf->internal.cksum = TF_CksumEnd<TF_CKSUM_TYPE>(tf->internal.cksum); // CKSUM_FINALIZE
                if (!tf->internal.discard_data) {
                    if (tf->internal.cksum == tf->internal.ref_cksum) {
                        TF_HandleReceivedMessage(tf);
                    } else {
                        TF_Error("Body cksum mismatch");
                    }
                }

                TF_ResetParser(tf);
            }
            break;
    }
    //@formatter:on
}

//endregion Parser


//region Compose and send

// Helper macros for the Compose functions
// use variables: si - signed int, b - byte, outbuff - target buffer, pos - count of bytes in buffer


/**
 * Write a number to the output buffer.
 *
 * @param type - data type
 * @param num - number to write
 * @param xtra - extra callback run after each byte, 'b' now contains the byte.
 */
#define WRITENUM_BASE(type, num, xtra) \
    for (si = sizeof(type)-1; si>=0; si--) { \
        b = (uint8_t)((num) >> (si*8) & 0xFF); \
        outbuff[pos++] = b; \
        xtra; \
    }

/**
 * Do nothing
 */
#define _NOOP()

/**
 * Write a number without adding its bytes to the checksum
 *
 * @param type - data type
 * @param num - number to write
 */
#define WRITENUM(type, num)       WRITENUM_BASE(type, num, _NOOP())

/**
 * Write a number AND add its bytes to the checksum
 *
 * @param type - data type
 * @param num - number to write
 */
#define WRITENUM_CKSUM(type, num) WRITENUM_BASE(type, num, TF_CksumAdd(cksum, b))

/**
 * Compose a frame (used internally by TF_Send and TF_Respond).
 * The frame can be sent using TF_WriteImpl(), or received by TF_Accept()
 *
 * @param outbuff - buffer to store the result in
 * @param msg - message written to the buffer
 * @return nr of bytes in outbuff used by the frame, 0 on failure
 */
template<TF_CKSUM_t TF_CKSUM_TYPE>
uint32_t _TF_FN TinyFrame<TF_CKSUM_TYPE>::TF_ComposeHead(TinyFrame *tf, uint8_t *outbuff, TF_Msg *msg)
{
    int8_t si = 0; // signed small int
    uint8_t b = 0;
    TF_ID id = 0;
    TF_CKSUM<TF_CKSUM_TYPE> cksum = 0;
    uint32_t pos = 0;

    (void)cksum; // suppress "unused" warning if checksums are disabled

    cksum = TF_CksumStart<TF_CKSUM_TYPE>(); // CKSUM_RESET

    // Gen ID
    if (msg->is_response) {
        id = msg->frame_id;
    }
    else {
        id = (TF_ID) (tf->internal.next_id++ & TF_ID_MASK);
        if (tf->internal.peer_bit) {
            id |= TF_ID_PEERBIT;
        }
    }

    msg->frame_id = id; // put the resolved ID into the message object for later use

    // --- Start ---
    cksum = TF_CksumStart<TF_CKSUM_TYPE>(); // CKSUM_RESET

#if TF_USE_SOF_BYTE
    outbuff[pos++] = TF_SOF_BYTE;
    cksum = TF_CksumAdd<TF_CKSUM_TYPE>(cksum, TF_SOF_BYTE); // CKSUM_ADD
#endif

    WRITENUM_CKSUM(TF_ID, id);
    WRITENUM_CKSUM(TF_LEN, msg->len);
    WRITENUM_CKSUM(TF_TYPE, msg->type);

#if TF_CKSUM_TYPE != TF_CKSUM_NONE
    CKSUM_FINALIZE(cksum);
    WRITENUM(TF_CKSUM, cksum);
#endif

    return pos;
}

/**
 * Compose a frame (used internally by TF_Send and TF_Respond).
 * The frame can be sent using TF_WriteImpl(), or received by TF_Accept()
 *
 * @param outbuff - buffer to store the result in
 * @param data - data buffer
 * @param data_len - data buffer len
 * @param cksum - checksum variable, used for all calls to TF_ComposeBody. Must be reset before first use! (CKSUM_RESET(cksum);)
 * @return nr of bytes in outbuff used
 */
template<TF_CKSUM_t TF_CKSUM_TYPE>
uint32_t _TF_FN TinyFrame<TF_CKSUM_TYPE>::TF_ComposeBody(uint8_t *outbuff,
                                    const uint8_t *data, TF_LEN data_len,
                                    TF_CKSUM<TF_CKSUM_TYPE> *cksum)
{
    TF_LEN i = 0;
    uint8_t b = 0;
    uint32_t pos = 0;

    for (i = 0; i < data_len; i++) {
        b = data[i];
        outbuff[pos++] = b;
        *cksum = TF_CksumAdd<TF_CKSUM_TYPE>(*cksum, b); // CKSUM_ADD
    }

    return pos;
}

/**
 * Finalize a frame
 *
 * @param outbuff - buffer to store the result in
 * @param cksum - checksum variable used for the body
 * @return nr of bytes in outbuff used
 */
template<TF_CKSUM_t TF_CKSUM_TYPE>
uint32_t _TF_FN TinyFrame<TF_CKSUM_TYPE>::TF_ComposeTail(uint8_t *outbuff, TF_CKSUM<TF_CKSUM_TYPE> *cksum)
{
    int8_t si = 0; // signed small int
    uint8_t b = 0;
    uint32_t pos = 0;

#if TF_CKSUM_TYPE != TF_CKSUM_NONE
    CKSUM_FINALIZE(*cksum);
    WRITENUM(TF_CKSUM, *cksum);
#endif
    return pos;
}

/**
 * Begin building and sending a frame
 *
 * @param tf - instance
 * @param msg - message to send
 * @param listener - response listener or nullptr
 * @param ftimeout - time out callback
 * @param timeout - listener timeout ticks, 0 = indefinite
 * @return success (mutex claimed and listener added, if any)
 */
template<TF_CKSUM_t TF_CKSUM_TYPE>
bool _TF_FN TinyFrame<TF_CKSUM_TYPE>::TF_SendFrame_Begin(TinyFrame *tf, TF_Msg *msg, TF_Listener listener, TF_Listener_Timeout ftimeout, TF_TICKS timeout)
{
    TF_TRY(TF_ClaimTx(tf));

    tf->internal.tx_pos = (uint32_t) TF_ComposeHead(tf, tf->internal.sendbuf, msg); // frame ID is incremented here if it's not a response
    tf->internal.tx_len = msg->len;

    if (listener) {
        if(!TF_AddIdListener(tf, msg, listener, ftimeout, timeout)) {
            TF_ReleaseTx(tf);
            return false;
        }
    }

    tf->internal.tx_cksum = TF_CksumStart<TF_CKSUM_TYPE>(); // CKSUM_RESET
    return true;
}

/**
 * Build and send a part (or all) of a frame body.
 * Caution: this does not check the total length against the length specified in the frame head
 *
 * @param tf - instance
 * @param buff - bytes to write
 * @param length - count
 */
template<TF_CKSUM_t TF_CKSUM_TYPE>
void _TF_FN TinyFrame<TF_CKSUM_TYPE>::TF_SendFrame_Chunk(TinyFrame *tf, const uint8_t *buff, uint32_t length)
{
    uint32_t remain;
    uint32_t chunk;
    uint32_t sent = 0;

    remain = length;
    while (remain > 0) {
        // Write what can fit in the tx buffer
        chunk = TF_MIN(TF_SENDBUF_LEN - tf->internal.tx_pos, remain);
        tf->internal.tx_pos += TF_ComposeBody(tf->internal.sendbuf+tf->internal.tx_pos, buff+sent, (TF_LEN) chunk, &tf->internal.tx_cksum);
        remain -= chunk;
        sent += chunk;

        // Flush if the buffer is full
        if (tf->internal.tx_pos == TF_SENDBUF_LEN) {
            TF_WriteImpl(tf, (const uint8_t *) tf->internal.sendbuf, tf->internal.tx_pos);
            tf->internal.tx_pos = 0;
        }
    }
}

/**
 * End a multi-part frame. This sends the checksum and releases mutex.
 *
 * @param tf - instance
 */
template<TF_CKSUM_t TF_CKSUM_TYPE>
void _TF_FN TinyFrame<TF_CKSUM_TYPE>::TF_SendFrame_End(TinyFrame *tf)
{
    // Checksum only if message had a body
    if (tf->internal.tx_len > 0) {
        // Flush if checksum wouldn't fit in the buffer
        if (TF_SENDBUF_LEN - tf->internal.tx_pos < sizeof(TF_CKSUM<TF_CKSUM_TYPE>)) {
            TF_WriteImpl(tf, (const uint8_t *) tf->internal.sendbuf, tf->internal.tx_pos);
            tf->internal.tx_pos = 0;
        }

        // Add checksum, flush what remains to be sent
        tf->internal.tx_pos += TF_ComposeTail(tf->internal.sendbuf + tf->internal.tx_pos, &tf->internal.tx_cksum);
    }

    TF_WriteImpl(tf, (const uint8_t *) tf->internal.sendbuf, tf->internal.tx_pos);
    TF_ReleaseTx(tf);
}

/**
 * Send a message
 *
 * @param tf - instance
 * @param msg - message object
 * @param listener - ID listener, or nullptr
 * @param ftimeout - time out callback
 * @param timeout - listener timeout, 0 is none
 * @return true if sent
 */
template<TF_CKSUM_t TF_CKSUM_TYPE>
bool _TF_FN TinyFrame<TF_CKSUM_TYPE>::TF_SendFrame(TinyFrame *tf, TF_Msg *msg, TF_Listener listener, TF_Listener_Timeout ftimeout, TF_TICKS timeout)
{
    TF_TRY(TF_SendFrame_Begin(tf, msg, listener, ftimeout, timeout));
    if (msg->len == 0 || msg->data != nullptr) {
        // Send the payload and checksum only if we're not starting a multi-part frame.
        // A multi-part frame is identified by passing nullptr to the data field and setting the length.
        // User then needs to call those functions manually
        TF_SendFrame_Chunk(tf, msg->data, msg->len);
        TF_SendFrame_End(tf);
    }
    return true;
}

//endregion Compose and send


//region Sending API funcs

/** send without listener */
template<TF_CKSUM_t TF_CKSUM_TYPE>
bool _TF_FN TinyFrame<TF_CKSUM_TYPE>::TF_Send(TinyFrame *tf, TF_Msg *msg)
{
    return TF_SendFrame(tf, msg, nullptr, nullptr, 0);
}

/** send without listener and struct */
template<TF_CKSUM_t TF_CKSUM_TYPE>
bool _TF_FN TinyFrame<TF_CKSUM_TYPE>::TF_SendSimple(TinyFrame *tf, TF_TYPE type, const uint8_t *data, TF_LEN len)
{
    TF_Msg msg;
    TF_ClearMsg(&msg);
    msg.type = type;
    msg.data = data;
    msg.len = len;
    return TF_Send(tf, &msg);
}

/** send with a listener waiting for a reply, without the struct */
template<TF_CKSUM_t TF_CKSUM_TYPE>
bool _TF_FN TinyFrame<TF_CKSUM_TYPE>::TF_QuerySimple(TinyFrame *tf, TF_TYPE type, const uint8_t *data, TF_LEN len, TF_Listener listener, TF_Listener_Timeout ftimeout, TF_TICKS timeout)
{
    TF_Msg msg;
    TF_ClearMsg(&msg);
    msg.type = type;
    msg.data = data;
    msg.len = len;
    return TF_SendFrame(tf, &msg, listener, ftimeout, timeout);
}

/** send with a listener waiting for a reply */
template<TF_CKSUM_t TF_CKSUM_TYPE>
bool _TF_FN TinyFrame<TF_CKSUM_TYPE>::TF_Query(TinyFrame *tf, TF_Msg *msg, TF_Listener listener, TF_Listener_Timeout ftimeout, TF_TICKS timeout)
{
    return TF_SendFrame(tf, msg, listener, ftimeout, timeout);
}

/** Like TF_Send, but with explicit frame ID (set inside the msg object), use for responses */
template<TF_CKSUM_t TF_CKSUM_TYPE>
bool _TF_FN TinyFrame<TF_CKSUM_TYPE>::TF_Respond(TinyFrame *tf, TF_Msg *msg)
{
    msg->is_response = true;
    return TF_Send(tf, msg);
}

//endregion Sending API funcs


//region Sending API funcs - multipart
template<TF_CKSUM_t TF_CKSUM_TYPE>
bool _TF_FN TinyFrame<TF_CKSUM_TYPE>::TF_Send_Multipart(TinyFrame *tf, TF_Msg *msg)
{
    msg->data = nullptr;
    return TF_Send(tf, msg);
}

template<TF_CKSUM_t TF_CKSUM_TYPE>
bool _TF_FN TinyFrame<TF_CKSUM_TYPE>::TF_SendSimple_Multipart(TinyFrame *tf, TF_TYPE type, TF_LEN len)
{
    return TF_SendSimple(tf, type, nullptr, len);
}

template<TF_CKSUM_t TF_CKSUM_TYPE>
bool _TF_FN TinyFrame<TF_CKSUM_TYPE>::TF_QuerySimple_Multipart(TinyFrame *tf, TF_TYPE type, TF_LEN len, TF_Listener listener, TF_Listener_Timeout ftimeout, TF_TICKS timeout)
{
    return TF_QuerySimple(tf, type, nullptr, len, listener, ftimeout, timeout);
}

template<TF_CKSUM_t TF_CKSUM_TYPE>
bool _TF_FN TinyFrame<TF_CKSUM_TYPE>::TF_Query_Multipart(TinyFrame *tf, TF_Msg *msg, TF_Listener listener, TF_Listener_Timeout ftimeout, TF_TICKS timeout)
{
    msg->data = nullptr;
    return TF_Query(tf, msg, listener, ftimeout, timeout);
}

template<TF_CKSUM_t TF_CKSUM_TYPE>
void _TF_FN TinyFrame<TF_CKSUM_TYPE>::TF_Respond_Multipart(TinyFrame *tf, TF_Msg *msg)
{
    msg->data = nullptr;
    TF_Respond(tf, msg);
}

template<TF_CKSUM_t TF_CKSUM_TYPE>
void _TF_FN TinyFrame<TF_CKSUM_TYPE>::TF_Multipart_Payload(TinyFrame *tf, const uint8_t *buff, uint32_t length)
{
    TF_SendFrame_Chunk(tf, buff, length);
}

template<TF_CKSUM_t TF_CKSUM_TYPE>
void _TF_FN TinyFrame<TF_CKSUM_TYPE>::TF_Multipart_Close(TinyFrame *tf)
{
    TF_SendFrame_End(tf);
}

//endregion Sending API funcs - multipart


/** Timebase hook - for timeouts */
template<TF_CKSUM_t TF_CKSUM_TYPE>
void _TF_FN TinyFrame<TF_CKSUM_TYPE>::TF_Tick()
{
    TF_COUNT i;
    TF_IdListener_ *lst;

    // increment parser timeout (timeout is handled when receiving next byte)
    if (this->internal.parser_timeout_ticks < TF_PARSER_TIMEOUT_TICKS) {
        this->internal.parser_timeout_ticks++;
    }

    // decrement and expire ID listeners
    for (i = 0; i < this->internal.count_id_lst; i++) {
        lst = &this->internal.id_listeners[i];
        if (!lst->fn || lst->timeout == 0) continue;
        // count down...
        if (--lst->timeout == 0) {
            TF_Error("ID listener %d has expired", (int)lst->id);
            if (lst->fn_timeout != nullptr) {
                lst->fn_timeout(this); // execute timeout function
            }
            // Listener has expired
            cleanup_id_listener(this, i, lst);
        }
    }
}

} // TinyFrame_n

#endif // TinyFrameH
