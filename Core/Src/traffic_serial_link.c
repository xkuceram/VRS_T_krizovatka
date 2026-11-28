#include "traffic_serial_link.h"
#include "traffic_fsm.h"

/* ---------------------------------------------------------------------------
 * Preklad typov protokolu na typy automatu a naspat
 * ------------------------------------------------------------------------- */

static TrafficMode to_fsm_mode(SerialMode mode)
{
    return (mode == SERIAL_MODE_CONTROL) ? TRAFFIC_MODE_CONTROL : TRAFFIC_MODE_DEMO;
}

static SerialMode from_fsm_mode(TrafficMode mode)
{
    switch (mode)
    {
        case TRAFFIC_MODE_DEMO:    return SERIAL_MODE_DEMO;
        case TRAFFIC_MODE_CONTROL: return SERIAL_MODE_CONTROL;
        default:                   return SERIAL_MODE_NONE;
    }
}

static TrafficTarget to_fsm_target(SerialTarget target)
{
    switch (target)
    {
        case SERIAL_TARGET_A:   return TRAFFIC_TARGET_A;
        case SERIAL_TARGET_A_S: return TRAFFIC_TARGET_A_S;
        case SERIAL_TARGET_B_R: return TRAFFIC_TARGET_B_R;
        case SERIAL_TARGET_B_D: return TRAFFIC_TARGET_B_D;
        case SERIAL_TARGET_C:   return TRAFFIC_TARGET_C;
        case SERIAL_TARGET_C_S: return TRAFFIC_TARGET_C_S;
        case SERIAL_TARGET_P:   return TRAFFIC_TARGET_P;
        case SERIAL_TARGET_B:   return TRAFFIC_TARGET_B;
        default:                return TRAFFIC_TARGET_COUNT;   /* automat vrati ERR_PARAM */
    }
}

static TrafficColor to_fsm_color(SerialColor color)
{
    switch (color)
    {
        case SERIAL_COLOR_GREEN:  return TRAFFIC_COLOR_GREEN;
        case SERIAL_COLOR_YELLOW: return TRAFFIC_COLOR_YELLOW;
        default:                  return TRAFFIC_COLOR_RED;
    }
}

static SerialColor from_fsm_color(TrafficColor color)
{
    switch (color)
    {
        case TRAFFIC_COLOR_GREEN:  return SERIAL_COLOR_GREEN;
        case TRAFFIC_COLOR_YELLOW: return SERIAL_COLOR_YELLOW;
        default:                   return SERIAL_COLOR_RED;
    }
}

static SerialResult from_fsm_result(TrafficResult result)
{
    switch (result)
    {
        case TRAFFIC_RESULT_OK:           return SERIAL_RESULT_OK;
        case TRAFFIC_RESULT_ERR_BUSY:     return SERIAL_RESULT_ERR_BUSY;
        case TRAFFIC_RESULT_ERR_MODE:     return SERIAL_RESULT_ERR_MODE;
        case TRAFFIC_RESULT_ERR_STATE:    return SERIAL_RESULT_ERR_STATE;
        case TRAFFIC_RESULT_ERR_CONFLICT: return SERIAL_RESULT_ERR_CONFLICT;
        default:                          return SERIAL_RESULT_ERR_SYNTAX;  /* ERR_PARAM */
    }
}

/* ---------------------------------------------------------------------------
 * Funkcie volane protokolom
 * ------------------------------------------------------------------------- */

static SerialResult link_request_mode(SerialMode mode)
{
    return from_fsm_result(TrafficFsm_RequestMode(to_fsm_mode(mode)));
}

static SerialResult link_request_signal(SerialTarget target, SerialColor color)
{
    return from_fsm_result(TrafficFsm_RequestSignal(to_fsm_target(target), to_fsm_color(color)));
}

static void link_get_status(SerialStatus *out)
{
    TrafficFsmStatus st;
    TrafficFsm_GetStatus(&st);

    out->mode  = from_fsm_mode(st.mode);
    out->next  = from_fsm_mode(st.next_mode);
    out->ready = st.ready ? 1U : 0U;
    out->a     = from_fsm_color(st.signals.a);
    out->b_r   = from_fsm_color(st.signals.b_r);
    out->b_d   = from_fsm_color(st.signals.b_d);
    out->c     = from_fsm_color(st.signals.c);
    out->p     = from_fsm_color(st.signals.p);
    out->a_s   = st.signals.a_s_on ? 1U : 0U;
    out->c_s   = st.signals.c_s_on ? 1U : 0U;
    out->car_c = st.car_c ? 1U : 0U;
    out->p_req = st.p_req ? 1U : 0U;
}

static const TrafficSerialHandlers link_handlers =
{
    link_request_mode,
    link_request_signal,
    link_get_status
};

const TrafficSerialHandlers *TrafficSerialLink_Handlers(void)
{
    return &link_handlers;
}
