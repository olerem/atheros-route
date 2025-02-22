/*
 * Copyright (c) 2007-2009 Atheros Communications, Inc.
 * All rights reserved.
 *
 */
/**
 * @defgroup isis_port_vlan ISIS_PORT_VLAN
 * @{
 */
#include "sw.h"
#include "hsl.h"
#include "hsl_dev.h"
#include "hsl_port_prop.h"
#include "isis_portvlan.h"
#include "isis_reg.h"

#define MAX_VLAN_ID          4095
#define ISIS_MAX_VLAN_TRANS  64
#define ISIS_VLAN_TRANS_ADDR 0x5ac00


static sw_error_t
_isis_port_route_defv_set(a_uint32_t dev_id, fal_port_t port_id)
{
    sw_error_t rv;
    a_uint32_t data, reg;

    HSL_REG_FIELD_GET(rv, dev_id, PORT_VLAN1, port_id,
                      COREP_EN, (a_uint8_t *) (&data), sizeof (a_uint32_t));

    SW_RTN_ON_ERROR(rv);

    if (data) {
        HSL_REG_FIELD_GET(rv, dev_id, PORT_VLAN0, port_id,
                      DEF_SVID, (a_uint8_t *) (&data), sizeof (a_uint32_t));
    } else {
        HSL_REG_FIELD_GET(rv, dev_id, PORT_VLAN0, port_id,
                      DEF_CVID, (a_uint8_t *) (&data), sizeof (a_uint32_t));
    }

    HSL_REG_ENTRY_GET(rv, dev_id, ROUTER_DEFV, (port_id / 2),
                      (a_uint8_t *) (&reg), sizeof (a_uint32_t));
    SW_RTN_ON_ERROR(rv);

    if (port_id % 2) {
        reg &= 0xffff;
        reg |= ((data & 0xfff) << 16);
    } else {
       reg &= 0xffff0000;
       reg |= (data & 0xfff);
    }

    HSL_REG_ENTRY_SET(rv, dev_id, ROUTER_DEFV, (port_id / 2),
                      (a_uint8_t *) (&reg), sizeof (a_uint32_t));
    return rv;
}

static sw_error_t
_isis_port_1qmode_set(a_uint32_t dev_id, fal_port_t port_id,
                      fal_pt_1qmode_t port_1qmode)
{
    sw_error_t rv;
    a_uint32_t data, regval[FAL_1Q_MODE_BUTT] = { 0, 3, 2, 1 };

    HSL_DEV_ID_CHECK(dev_id);

    if (A_FALSE == hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU)) {
        return SW_BAD_PARAM;
    }

    if (FAL_1Q_MODE_BUTT <= port_1qmode) {
        return SW_BAD_PARAM;
    }

    HSL_REG_FIELD_SET(rv, dev_id, PORT_LOOKUP_CTL, port_id, DOT1Q_MODE,
                      (a_uint8_t *) (&regval[port_1qmode]),
                      sizeof (a_uint32_t));
    SW_RTN_ON_ERROR(rv);

    if (FAL_1Q_DISABLE == port_1qmode) {
        data = 1;
    } else {
        data = 0;
    }

    HSL_REG_FIELD_SET(rv, dev_id, PORT_VLAN1, port_id, VLAN_DIS,
                      (a_uint8_t *) (&data), sizeof (a_uint32_t));
    return rv;
}

static sw_error_t
_isis_port_1qmode_get(a_uint32_t dev_id, fal_port_t port_id,
                      fal_pt_1qmode_t * pport_1qmode)
{
    sw_error_t rv;
    a_uint32_t regval = 0;
    fal_pt_1qmode_t retval[4] = { FAL_1Q_DISABLE, FAL_1Q_FALLBACK,
        FAL_1Q_CHECK, FAL_1Q_SECURE
    };

    HSL_DEV_ID_CHECK(dev_id);

    if (A_FALSE == hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU)) {
        return SW_BAD_PARAM;
    }

    SW_RTN_ON_NULL(pport_1qmode);

    HSL_REG_FIELD_GET(rv, dev_id, PORT_LOOKUP_CTL, port_id, DOT1Q_MODE,
                      (a_uint8_t *) (&regval), sizeof (a_uint32_t));

    SW_RTN_ON_ERROR(rv);

    *pport_1qmode = retval[regval & 0x3];

    return SW_OK;
}

static sw_error_t
_isis_port_egvlanmode_set(a_uint32_t dev_id, fal_port_t port_id,
                          fal_pt_1q_egmode_t port_egvlanmode)
{
    sw_error_t rv;
    a_uint32_t data, regval[FAL_EG_MODE_BUTT] = { 0, 1, 2, 3, 3 };

    HSL_DEV_ID_CHECK(dev_id);

    if (A_FALSE == hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU)) {
        return SW_BAD_PARAM;
    }

    if ((FAL_EG_MODE_BUTT <= port_egvlanmode) 
        || (FAL_EG_HYBRID == port_egvlanmode)) {
        return SW_BAD_PARAM;
    }

    HSL_REG_FIELD_SET(rv, dev_id, PORT_VLAN1, port_id, EG_VLAN_MODE,
                      (a_uint8_t *) (&regval[port_egvlanmode]),
                      sizeof (a_uint32_t));

    SW_RTN_ON_ERROR(rv);

    HSL_REG_ENTRY_GET(rv, dev_id, ROUTER_EG, 0,
                      (a_uint8_t *) (&data), sizeof (a_uint32_t));
    SW_RTN_ON_ERROR(rv);

    data &= (~(0x3 << (port_id << 2)));
    data |= (regval[port_egvlanmode] << (port_id << 2));

    HSL_REG_ENTRY_SET(rv, dev_id, ROUTER_EG, 0,
                      (a_uint8_t *) (&data), sizeof (a_uint32_t));
    return rv;
}

static sw_error_t
_isis_port_egvlanmode_get(a_uint32_t dev_id, fal_port_t port_id,
                          fal_pt_1q_egmode_t * pport_egvlanmode)
{
    sw_error_t rv;
    a_uint32_t regval = 0;
    fal_pt_1q_egmode_t retval[4] = { FAL_EG_UNMODIFIED, FAL_EG_UNTAGGED,
        FAL_EG_TAGGED, FAL_EG_UNTOUCHED
    };

    HSL_DEV_ID_CHECK(dev_id);

    if (A_FALSE == hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU)) {
        return SW_BAD_PARAM;
    }

    SW_RTN_ON_NULL(pport_egvlanmode);

    HSL_REG_FIELD_GET(rv, dev_id, PORT_VLAN1, port_id, EG_VLAN_MODE,
                      (a_uint8_t *) (&regval), sizeof (a_uint32_t));

    SW_RTN_ON_ERROR(rv);

    *pport_egvlanmode = retval[regval & 0x3];

    return SW_OK;
}

static sw_error_t
_isis_portvlan_member_add(a_uint32_t dev_id, fal_port_t port_id,
                          a_uint32_t mem_port_id)
{
    sw_error_t rv;
    a_uint32_t regval = 0;

    HSL_DEV_ID_CHECK(dev_id);

    if (A_FALSE == hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU)) {
        return SW_BAD_PARAM;
    }

    if (A_FALSE == hsl_port_prop_check(dev_id, mem_port_id, HSL_PP_INCL_CPU)) {
        return SW_BAD_PARAM;
    }

    HSL_REG_FIELD_GET(rv, dev_id, PORT_LOOKUP_CTL, port_id,
                      PORT_VID_MEM, (a_uint8_t *) (&regval),
                      sizeof (a_uint32_t));

    SW_RTN_ON_ERROR(rv);

    regval |= (0x1UL << mem_port_id);

    HSL_REG_FIELD_SET(rv, dev_id, PORT_LOOKUP_CTL, port_id,
                      PORT_VID_MEM, (a_uint8_t *) (&regval),
                      sizeof (a_uint32_t));

    return rv;
}

static sw_error_t
_isis_portvlan_member_del(a_uint32_t dev_id, fal_port_t port_id,
                          a_uint32_t mem_port_id)
{
    sw_error_t rv;
    a_uint32_t regval = 0;

    HSL_DEV_ID_CHECK(dev_id);

    if (A_FALSE == hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU)) {
        return SW_BAD_PARAM;
    }

    if (A_FALSE == hsl_port_prop_check(dev_id, mem_port_id, HSL_PP_INCL_CPU)) {
        return SW_BAD_PARAM;
    }

    HSL_REG_FIELD_GET(rv, dev_id, PORT_LOOKUP_CTL, port_id,
                      PORT_VID_MEM, (a_uint8_t *) (&regval),
                      sizeof (a_uint32_t));

    SW_RTN_ON_ERROR(rv);

    regval &= (~(0x1UL << mem_port_id));

    HSL_REG_FIELD_SET(rv, dev_id, PORT_LOOKUP_CTL, port_id,
                      PORT_VID_MEM, (a_uint8_t *) (&regval),
                      sizeof (a_uint32_t));

    return rv;
}

static sw_error_t
_isis_portvlan_member_update(a_uint32_t dev_id, fal_port_t port_id,
                             fal_pbmp_t mem_port_map)
{
    sw_error_t rv;

    HSL_DEV_ID_CHECK(dev_id);

    if (A_FALSE == hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU)) {
        return SW_BAD_PARAM;
    }

    if (A_FALSE == hsl_mports_prop_check(dev_id, mem_port_map, HSL_PP_INCL_CPU)) {
        return SW_BAD_PARAM;
    }

    HSL_REG_FIELD_SET(rv, dev_id, PORT_LOOKUP_CTL, port_id,
                      PORT_VID_MEM, (a_uint8_t *) (&mem_port_map),
                      sizeof (a_uint32_t));

    return rv;
}

static sw_error_t
_isis_portvlan_member_get(a_uint32_t dev_id, fal_port_t port_id,
                          fal_pbmp_t * mem_port_map)
{
    sw_error_t rv;

    HSL_DEV_ID_CHECK(dev_id);

    if (A_FALSE == hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU)) {
        return SW_BAD_PARAM;
    }

    *mem_port_map = 0;
    HSL_REG_FIELD_GET(rv, dev_id, PORT_LOOKUP_CTL, port_id,
                      PORT_VID_MEM, (a_uint8_t *) mem_port_map,
                      sizeof (a_uint32_t));

    SW_RTN_ON_ERROR(rv);

    return SW_OK;
}

static sw_error_t
_isis_port_force_default_vid_set(a_uint32_t dev_id, fal_port_t port_id,
                                 a_bool_t enable)
{
    sw_error_t rv;
    a_uint32_t val;

    HSL_DEV_ID_CHECK(dev_id);

    if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU)) {
        return SW_BAD_PARAM;
    }

    if (A_TRUE == enable) {
        val = 1;
    } else if (A_FALSE == enable) {
        val = 0;
    } else {
        return SW_BAD_PARAM;
    }

    HSL_REG_FIELD_SET(rv, dev_id, PORT_VLAN1, port_id,
                      FORCE_DEF_VID, (a_uint8_t *) (&val), sizeof (a_uint32_t));
    return rv;
}

static sw_error_t
_isis_port_force_default_vid_get(a_uint32_t dev_id, fal_port_t port_id,
                                 a_bool_t * enable)
{
    sw_error_t rv;
    a_uint32_t val;

    HSL_DEV_ID_CHECK(dev_id);

    if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU)) {
        return SW_BAD_PARAM;
    }

    HSL_REG_FIELD_GET(rv, dev_id, PORT_VLAN1, port_id,
                      FORCE_DEF_VID, (a_uint8_t *) (&val), sizeof (a_uint32_t));
    SW_RTN_ON_ERROR(rv);

    if (val) {
        *enable = A_TRUE;
    } else {
        *enable = A_FALSE;
    }

    return SW_OK;
}

static sw_error_t
_isis_port_force_portvlan_set(a_uint32_t dev_id, fal_port_t port_id,
                              a_bool_t enable)
{
    sw_error_t rv;
    a_uint32_t val;

    HSL_DEV_ID_CHECK(dev_id);

    if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU)) {
        return SW_BAD_PARAM;
    }

    if (A_TRUE == enable) {
        val = 1;
    } else if (A_FALSE == enable) {
        val = 0;
    } else {
        return SW_BAD_PARAM;
    }

    HSL_REG_FIELD_SET(rv, dev_id, PORT_LOOKUP_CTL, port_id,
                      FORCE_PVLAN, (a_uint8_t *) (&val), sizeof (a_uint32_t));
    return rv;
}

static sw_error_t
_isis_port_force_portvlan_get(a_uint32_t dev_id, fal_port_t port_id,
                              a_bool_t * enable)
{
    sw_error_t rv;
    a_uint32_t val;

    HSL_DEV_ID_CHECK(dev_id);

    if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU)) {
        return SW_BAD_PARAM;
    }

    HSL_REG_FIELD_GET(rv, dev_id, PORT_LOOKUP_CTL, port_id,
                      FORCE_PVLAN, (a_uint8_t *) (&val), sizeof (a_uint32_t));
    SW_RTN_ON_ERROR(rv);

    if (val) {
        *enable = A_TRUE;
    } else {
        *enable = A_FALSE;
    }

    return SW_OK;
}

static sw_error_t
_isis_nestvlan_tpid_set(a_uint32_t dev_id, a_uint32_t tpid)
{
    sw_error_t rv;
    a_uint32_t val;

    HSL_DEV_ID_CHECK(dev_id);

    val = tpid;
    HSL_REG_FIELD_SET(rv, dev_id, SERVICE_TAG, 0,
                      TAG_VALUE, (a_uint8_t *) (&val), sizeof (a_uint32_t));
    return rv;
}

static sw_error_t
_isis_nestvlan_tpid_get(a_uint32_t dev_id, a_uint32_t * tpid)
{
    sw_error_t rv;
    a_uint32_t val;

    HSL_DEV_ID_CHECK(dev_id);

    HSL_REG_FIELD_GET(rv, dev_id, SERVICE_TAG, 0,
                      TAG_VALUE, (a_uint8_t *) (&val), sizeof (a_uint32_t));
    SW_RTN_ON_ERROR(rv);

    *tpid = val;
    return SW_OK;
}

static sw_error_t
_isis_port_invlan_mode_set(a_uint32_t dev_id, fal_port_t port_id,
                           fal_pt_invlan_mode_t mode)
{
    sw_error_t rv;
    a_uint32_t regval[FAL_INVLAN_MODE_BUTT] = { 0, 1, 2 };

    HSL_DEV_ID_CHECK(dev_id);

    if (A_FALSE == hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU)) {
        return SW_BAD_PARAM;
    }

    if (FAL_INVLAN_MODE_BUTT <= mode) {
        return SW_BAD_PARAM;
    }

    HSL_REG_FIELD_SET(rv, dev_id, PORT_VLAN1, port_id, IN_VLAN_MODE,
                      (a_uint8_t *) (&regval[mode]), sizeof (a_uint32_t));
    return rv;
}

static sw_error_t
_isis_port_invlan_mode_get(a_uint32_t dev_id, fal_port_t port_id,
                           fal_pt_invlan_mode_t * mode)
{
    sw_error_t rv;
    a_uint32_t regval = 0;
    fal_pt_invlan_mode_t retval[FAL_INVLAN_MODE_BUTT] = { FAL_INVLAN_ADMIT_ALL,
        FAL_INVLAN_ADMIT_TAGGED, FAL_INVLAN_ADMIT_UNTAGGED
    };

    HSL_DEV_ID_CHECK(dev_id);

    if (A_FALSE == hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU)) {
        return SW_BAD_PARAM;
    }

    SW_RTN_ON_NULL(mode);

    HSL_REG_FIELD_GET(rv, dev_id, PORT_VLAN1, port_id, IN_VLAN_MODE,
                      (a_uint8_t *) (&regval), sizeof (a_uint32_t));

    SW_RTN_ON_ERROR(rv);

    if (regval >= 3) {
        return SW_FAIL;
    }
    *mode = retval[regval & 0x3];

    return rv;
}

static sw_error_t
_isis_port_tls_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
{
    sw_error_t rv;
    a_uint32_t val;

    HSL_DEV_ID_CHECK(dev_id);

    if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU)) {
        return SW_BAD_PARAM;
    }

    if (A_TRUE == enable) {
        val = 1;
    } else if (A_FALSE == enable) {
        val = 0;
    } else {
        return SW_BAD_PARAM;
    }

    HSL_REG_FIELD_SET(rv, dev_id, PORT_VLAN1, port_id,
                      TLS_EN, (a_uint8_t *) (&val), sizeof (a_uint32_t));
    return rv;
}

static sw_error_t
_isis_port_tls_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
{
    sw_error_t rv;
    a_uint32_t val;

    HSL_DEV_ID_CHECK(dev_id);

    if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU)) {
        return SW_BAD_PARAM;
    }

    HSL_REG_FIELD_GET(rv, dev_id, PORT_VLAN1, port_id,
                      TLS_EN, (a_uint8_t *) (&val), sizeof (a_uint32_t));
    SW_RTN_ON_ERROR(rv);

    if (1 == val) {
        *enable = A_TRUE;
    } else {
        *enable = A_FALSE;
    }

    return SW_OK;
}

static sw_error_t
_isis_port_pri_propagation_set(a_uint32_t dev_id, fal_port_t port_id,
                               a_bool_t enable)
{
    sw_error_t rv;
    a_uint32_t val;

    HSL_DEV_ID_CHECK(dev_id);

    if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU)) {
        return SW_BAD_PARAM;
    }

    if (A_TRUE == enable) {
        val = 1;
    } else if (A_FALSE == enable) {
        val = 0;
    } else {
        return SW_BAD_PARAM;
    }

    HSL_REG_FIELD_SET(rv, dev_id, PORT_VLAN1, port_id,
                      PRI_PROPAGATION, (a_uint8_t *) (&val),
                      sizeof (a_uint32_t));
    return rv;
}

static sw_error_t
_isis_port_pri_propagation_get(a_uint32_t dev_id, fal_port_t port_id,
                               a_bool_t * enable)
{
    sw_error_t rv;
    a_uint32_t val;

    HSL_DEV_ID_CHECK(dev_id);

    if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU)) {
        return SW_BAD_PARAM;
    }

    HSL_REG_FIELD_GET(rv, dev_id, PORT_VLAN1, port_id,
                      PRI_PROPAGATION, (a_uint8_t *) (&val),
                      sizeof (a_uint32_t));
    SW_RTN_ON_ERROR(rv);

    if (1 == val) {
        *enable = A_TRUE;
    } else {
        *enable = A_FALSE;
    }

    return SW_OK;
}

static sw_error_t
_isis_port_default_svid_set(a_uint32_t dev_id, fal_port_t port_id,
                            a_uint32_t vid)
{
    sw_error_t rv;
    a_uint32_t val;

    HSL_DEV_ID_CHECK(dev_id);

    if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU)) {
        return SW_BAD_PARAM;
    }

    if (vid > MAX_VLAN_ID) {
        return SW_BAD_PARAM;
    }

    val = vid;
    HSL_REG_FIELD_SET(rv, dev_id, PORT_VLAN0, port_id,
                      DEF_SVID, (a_uint8_t *) (&val), sizeof (a_uint32_t));

    SW_RTN_ON_ERROR(rv);

    rv = _isis_port_route_defv_set(dev_id, port_id);
    return rv;
}

static sw_error_t
_isis_port_default_svid_get(a_uint32_t dev_id, fal_port_t port_id,
                            a_uint32_t * vid)
{
    sw_error_t rv;
    a_uint32_t val;

    HSL_DEV_ID_CHECK(dev_id);

    if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU)) {
        return SW_BAD_PARAM;
    }

    HSL_REG_FIELD_GET(rv, dev_id, PORT_VLAN0, port_id,
                      DEF_SVID, (a_uint8_t *) (&val), sizeof (a_uint32_t));

    *vid = val & 0xfff;
    return rv;
}

static sw_error_t
_isis_port_default_cvid_set(a_uint32_t dev_id, fal_port_t port_id,
                            a_uint32_t vid)
{
    sw_error_t rv;
    a_uint32_t val;

    HSL_DEV_ID_CHECK(dev_id);

    if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU)) {
        return SW_BAD_PARAM;
    }

    if (vid > MAX_VLAN_ID) {
        return SW_BAD_PARAM;
    }

    val = vid;
    HSL_REG_FIELD_SET(rv, dev_id, PORT_VLAN0, port_id,
                      DEF_CVID, (a_uint8_t *) (&val), sizeof (a_uint32_t));

    SW_RTN_ON_ERROR(rv);

    rv = _isis_port_route_defv_set(dev_id, port_id);
    return rv;
}

static sw_error_t
_isis_port_default_cvid_get(a_uint32_t dev_id, fal_port_t port_id,
                            a_uint32_t * vid)
{
    sw_error_t rv;
    a_uint32_t val;

    HSL_DEV_ID_CHECK(dev_id);

    if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU)) {
        return SW_BAD_PARAM;
    }

    HSL_REG_FIELD_GET(rv, dev_id, PORT_VLAN0, port_id,
                      DEF_CVID, (a_uint8_t *) (&val), sizeof (a_uint32_t));

    *vid = val & 0xfff;
    return rv;
}

static sw_error_t
_isis_port_vlan_propagation_set(a_uint32_t dev_id, fal_port_t port_id,
                                fal_vlan_propagation_mode_t mode)
{
    sw_error_t rv;
    a_uint32_t reg, p, c;

    HSL_DEV_ID_CHECK(dev_id);

    if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU)) {
        return SW_BAD_PARAM;
    }

    if (FAL_VLAN_PROPAGATION_DISABLE == mode) {
        p = 0;
        c = 0;
    } else if (FAL_VLAN_PROPAGATION_CLONE == mode) {
        p = 1;
        c = 1;
    } else if (FAL_VLAN_PROPAGATION_REPLACE == mode) {
        p = 1;
        c = 0;
    } else {
        return SW_BAD_PARAM;
    }

    HSL_REG_ENTRY_GET(rv, dev_id, PORT_VLAN1, port_id,
                      (a_uint8_t *) (&reg), sizeof (a_uint32_t));
    SW_RTN_ON_ERROR(rv);

    SW_SET_REG_BY_FIELD(PORT_VLAN1, PROPAGATION_EN, p, reg);
    SW_SET_REG_BY_FIELD(PORT_VLAN1, CLONE, c, reg);

    HSL_REG_ENTRY_SET(rv, dev_id, PORT_VLAN1, port_id,
                      (a_uint8_t *) (&reg), sizeof (a_uint32_t));
    return rv;
}

static sw_error_t
_isis_port_vlan_propagation_get(a_uint32_t dev_id, fal_port_t port_id,
                                fal_vlan_propagation_mode_t * mode)
{
    sw_error_t rv;
    a_uint32_t reg, p, c;

    HSL_DEV_ID_CHECK(dev_id);

    if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU)) {
        return SW_BAD_PARAM;
    }

    HSL_REG_ENTRY_GET(rv, dev_id, PORT_VLAN1, port_id,
                      (a_uint8_t *) (&reg), sizeof (a_uint32_t));
    SW_RTN_ON_ERROR(rv);

    SW_GET_FIELD_BY_REG(PORT_VLAN1, PROPAGATION_EN, p, reg);
    SW_GET_FIELD_BY_REG(PORT_VLAN1, CLONE, c, reg);

    if (p) {
        if (c) {
            *mode = FAL_VLAN_PROPAGATION_CLONE;
        } else {
            *mode = FAL_VLAN_PROPAGATION_REPLACE;
        }
    } else {
        *mode = FAL_VLAN_PROPAGATION_DISABLE;
    }

    return SW_OK;
}

static sw_error_t
_isis_vlan_trans_read(a_uint32_t dev_id, a_uint32_t entry_idx,
                      fal_pbmp_t * pbmp, fal_vlan_trans_entry_t * entry)
{
    sw_error_t rv;
    a_uint32_t i, addr, dir, table[2];

    *pbmp = 0;
    aos_mem_zero(entry, sizeof (fal_vlan_trans_entry_t));

    addr = ISIS_VLAN_TRANS_ADDR + (entry_idx << 3);
    /* get vlan trans table */
    for (i = 0; i < 2; i++) {
        HSL_REG_ENTRY_GEN_GET(rv, dev_id, addr + (i << 2), sizeof (a_uint32_t),
                              (a_uint8_t *) (&(table[i])), sizeof (a_uint32_t));
        SW_RTN_ON_ERROR(rv);
    }

    dir = 0x3 & (table[1] >> 4);
    if (!dir) {
        return SW_EMPTY;
    }

    entry->o_vid = table[0] & 0xfff;
    *pbmp = (table[1] >> 6) & 0x7f;

    if (3 == dir) {
        entry->bi_dir = A_TRUE;
        entry->forward_dir = A_TRUE;
        entry->reverse_dir = A_TRUE;
    } else if (1 == dir) {
        entry->bi_dir = A_FALSE;
        entry->forward_dir = A_TRUE;
        entry->reverse_dir = A_FALSE;
    } else {
        entry->bi_dir = A_FALSE;
        entry->forward_dir = A_FALSE;
        entry->reverse_dir = A_TRUE;
    }

    entry->o_vid_is_cvid = (table[1] >> 13) & 0x1UL;
    entry->one_2_one_vlan = (table[1] >> 16) & 0x1UL;
    entry->s_vid_enable = (table[1] >> 14) & 0x1UL;
    entry->c_vid_enable = (table[1] >> 15) & 0x1UL;

    if (A_TRUE == entry->s_vid_enable) {
        entry->s_vid = (table[0] >> 12) & 0xfff;
    }

    if (A_TRUE == entry->c_vid_enable) {
        entry->c_vid = ((table[0] >> 24) & 0xff) | ((table[1] & 0xf) << 8);
    }

    return SW_OK;
}

static sw_error_t
_isis_vlan_trans_write(a_uint32_t dev_id, a_uint32_t entry_idx, fal_pbmp_t pbmp,
                       fal_vlan_trans_entry_t entry)
{
    sw_error_t rv;
    a_uint32_t i, addr, table[2] = { 0 };

    addr = ISIS_VLAN_TRANS_ADDR + (entry_idx << 3);

    if (0 != pbmp) {
        table[0] = entry.o_vid & 0xfff;
        table[0] |= ((entry.s_vid & 0xfff) << 12);
        table[0] |= ((entry.c_vid & 0xff) << 24);
        table[1] = (entry.c_vid >> 8) & 0xf;

        if (A_TRUE == entry.bi_dir) {
            table[1] |= (0x3 << 4);
        }

        if (A_TRUE == entry.forward_dir) {
            table[1] |= (0x1 << 4);
        }

        if (A_TRUE == entry.reverse_dir) {
            table[1] |= (0x1 << 5);
        }

        table[1] |= (pbmp << 6);
        table[1] |= ((0x1UL & entry.o_vid_is_cvid) << 13);
        table[1] |= ((0x1UL & entry.s_vid_enable) << 14);
        table[1] |= ((0x1UL & entry.c_vid_enable) << 15);
        table[1] |= ((0x1UL & entry.one_2_one_vlan) << 16);
    }

    /* set vlan trans table */
    for (i = 0; i < 2; i++) {
        HSL_REG_ENTRY_GEN_SET(rv, dev_id, addr + (i << 2), sizeof (a_uint32_t),
                              (a_uint8_t *) (&(table[i])), sizeof (a_uint32_t));
        SW_RTN_ON_ERROR(rv);
    }

    return SW_OK;
}

static sw_error_t
_isis_port_vlan_trans_convert(fal_vlan_trans_entry_t * entry,
                              fal_vlan_trans_entry_t * local)
{
    aos_mem_copy(local, entry, sizeof (fal_vlan_trans_entry_t));

    if ((A_TRUE == local->bi_dir)
        || ((A_TRUE == local->forward_dir)
            && (A_TRUE == local->reverse_dir))) {
        local->bi_dir = A_TRUE;
        local->forward_dir = A_TRUE;
        local->reverse_dir = A_TRUE;
    }

    if (A_FALSE == local->s_vid_enable) {
        local->s_vid = 0;
    }

    if (A_FALSE == local->c_vid_enable) {
        local->c_vid = 0;
    }

    return SW_OK;
}

static sw_error_t
_isis_port_vlan_trans_add(a_uint32_t dev_id, fal_port_t port_id,
                          fal_vlan_trans_entry_t * entry)
{
    sw_error_t rv;
    fal_pbmp_t t_pbmp;
    a_uint32_t idx, entry_idx = ISIS_MAX_VLAN_TRANS;
    fal_vlan_trans_entry_t temp, local;

    HSL_DEV_ID_CHECK(dev_id);

    if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU)) {
        return SW_BAD_PARAM;
    }

    rv = _isis_port_vlan_trans_convert(entry, &local);
    SW_RTN_ON_ERROR(rv);

    for (idx = 0; idx < ISIS_MAX_VLAN_TRANS; idx++) {
        rv = _isis_vlan_trans_read(dev_id, idx, &t_pbmp, &temp);
        if (SW_EMPTY == rv) {
            entry_idx = idx;
            continue;
        }
        SW_RTN_ON_ERROR(rv);

        if (!aos_mem_cmp(&local, &temp, sizeof (fal_vlan_trans_entry_t))) {
            if (SW_IS_PBMP_MEMBER(t_pbmp, port_id)) {
                return SW_ALREADY_EXIST;
            }
            entry_idx = idx;
            break;
        } else {
            t_pbmp = 0;
        }
    }

    if (ISIS_MAX_VLAN_TRANS != entry_idx) {
        t_pbmp |= (0x1 << port_id);
    } else {
        return SW_NO_RESOURCE;
    }

    return _isis_vlan_trans_write(dev_id, entry_idx, t_pbmp, local);
}

static sw_error_t
_isis_port_vlan_trans_del(a_uint32_t dev_id, fal_port_t port_id,
                          fal_vlan_trans_entry_t * entry)
{
    sw_error_t rv;
    fal_pbmp_t t_pbmp;
    a_uint32_t idx, entry_idx = ISIS_MAX_VLAN_TRANS;
    fal_vlan_trans_entry_t temp, local;

    HSL_DEV_ID_CHECK(dev_id);

    if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU)) {
        return SW_BAD_PARAM;
    }

    rv = _isis_port_vlan_trans_convert(entry, &local);
    SW_RTN_ON_ERROR(rv);

    for (idx = 0; idx < ISIS_MAX_VLAN_TRANS; idx++) {
        rv = _isis_vlan_trans_read(dev_id, idx, &t_pbmp, &temp);
        if (SW_EMPTY == rv) {
            continue;
        }
        SW_RTN_ON_ERROR(rv);

        if (!aos_mem_cmp(&temp, &local, sizeof (fal_vlan_trans_entry_t))) {
            if (SW_IS_PBMP_MEMBER(t_pbmp, port_id)) {
                entry_idx = idx;
                break;
            }
        }
    }

    if (ISIS_MAX_VLAN_TRANS != entry_idx) {
        t_pbmp &= (~(0x1 << port_id));
    } else {
        return SW_NOT_FOUND;
    }

    return _isis_vlan_trans_write(dev_id, entry_idx, t_pbmp, local);
}

static sw_error_t
_isis_port_vlan_trans_get(a_uint32_t dev_id, fal_port_t port_id,
                          fal_vlan_trans_entry_t * entry)
{
    sw_error_t rv;
    fal_pbmp_t t_pbmp;
    a_uint32_t idx;
    fal_vlan_trans_entry_t temp, local;

    HSL_DEV_ID_CHECK(dev_id);

    if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU)) {
        return SW_BAD_PARAM;
    }

    rv = _isis_port_vlan_trans_convert(entry, &local);
    SW_RTN_ON_ERROR(rv);

    for (idx = 0; idx < ISIS_MAX_VLAN_TRANS; idx++) {
        rv = _isis_vlan_trans_read(dev_id, idx, &t_pbmp, &temp);
        if (SW_EMPTY == rv) {
            continue;
        }
        SW_RTN_ON_ERROR(rv);

        if (!aos_mem_cmp(&temp, &local, sizeof (fal_vlan_trans_entry_t))) {
            if (SW_IS_PBMP_MEMBER(t_pbmp, port_id)) {
                return SW_OK;
            }
        }
    }

    return SW_NOT_FOUND;
}

static sw_error_t
_isis_port_vlan_trans_iterate(a_uint32_t dev_id, fal_port_t port_id,
                              a_uint32_t * iterator,
                              fal_vlan_trans_entry_t * entry)
{
    a_uint32_t index;
    sw_error_t rv;
    fal_vlan_trans_entry_t entry_t;
    fal_pbmp_t pbmp_t;

    HSL_DEV_ID_CHECK(dev_id);

    if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU)) {
        return SW_BAD_PARAM;
    }

    if ((NULL == iterator) || (NULL == entry)) {
        return SW_BAD_PTR;
    }

    if (ISIS_MAX_VLAN_TRANS < *iterator) {
        return SW_BAD_PARAM;
    }

    for (index = *iterator; index < ISIS_MAX_VLAN_TRANS; index++) {
        rv = _isis_vlan_trans_read(dev_id, index, &pbmp_t, &entry_t);
        if (SW_EMPTY == rv) {
            continue;
        }

        if (SW_IS_PBMP_MEMBER(pbmp_t, port_id)) {
            aos_mem_copy(entry, &entry_t, sizeof (fal_vlan_trans_entry_t));
            break;
        }
    }

    if (ISIS_MAX_VLAN_TRANS == index) {
        return SW_NO_MORE;
    }

    *iterator = index + 1;
    return SW_OK;
}

static sw_error_t
_isis_qinq_mode_set(a_uint32_t dev_id, fal_qinq_mode_t mode)
{
    sw_error_t rv;
    a_uint32_t stag = 0;

    HSL_DEV_ID_CHECK(dev_id);

    if (FAL_QINQ_MODE_BUTT <= mode) {
        return SW_BAD_PARAM;
    }

    if (FAL_QINQ_STAG_MODE == mode) {
        stag = 1;
    }

    HSL_REG_FIELD_SET(rv, dev_id, SERVICE_TAG, 0,
                      STAG_MODE, (a_uint8_t *) (&stag), sizeof (a_uint32_t));

    return rv;
}

static sw_error_t
_isis_qinq_mode_get(a_uint32_t dev_id, fal_qinq_mode_t * mode)
{
    sw_error_t rv;
    a_uint32_t stag = 0;

    HSL_DEV_ID_CHECK(dev_id);

    HSL_REG_FIELD_GET(rv, dev_id, SERVICE_TAG, 0,
                      STAG_MODE, (a_uint8_t *) (&stag), sizeof (a_uint32_t));

    SW_RTN_ON_ERROR(rv);

    if (stag) {
        *mode = FAL_QINQ_STAG_MODE;
    } else {
        *mode = FAL_QINQ_CTAG_MODE;
    }

    return SW_OK;
}

static sw_error_t
_isis_port_qinq_role_set(a_uint32_t dev_id, fal_port_t port_id,
                         fal_qinq_port_role_t role)
{
    sw_error_t rv;
    a_uint32_t core = 0;

    HSL_DEV_ID_CHECK(dev_id);

    if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU)) {
        return SW_BAD_PARAM;
    }

    if (FAL_QINQ_PORT_ROLE_BUTT <= role) {
        return SW_BAD_PARAM;
    }

    if (FAL_QINQ_CORE_PORT == role) {
        core = 1;
    }

    HSL_REG_FIELD_SET(rv, dev_id, PORT_VLAN1, port_id,
                      COREP_EN, (a_uint8_t *) (&core), sizeof (a_uint32_t));

    SW_RTN_ON_ERROR(rv);

    rv = _isis_port_route_defv_set(dev_id, port_id);
    return rv;
}

static sw_error_t
_isis_port_qinq_role_get(a_uint32_t dev_id, fal_port_t port_id,
                         fal_qinq_port_role_t * role)
{
    sw_error_t rv;
    a_uint32_t core = 0;

    HSL_DEV_ID_CHECK(dev_id);

    if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU)) {
        return SW_BAD_PARAM;
    }

    HSL_REG_FIELD_GET(rv, dev_id, PORT_VLAN1, port_id,
                      COREP_EN, (a_uint8_t *) (&core), sizeof (a_uint32_t));

    SW_RTN_ON_ERROR(rv);

    if (core) {
        *role = FAL_QINQ_CORE_PORT;
    } else {
        *role = FAL_QINQ_EDGE_PORT;
    }

    return SW_OK;
}

static sw_error_t
_isis_port_mac_vlan_xlt_set(a_uint32_t dev_id, fal_port_t port_id,
                        a_bool_t enable)
{
    sw_error_t rv;
    a_uint32_t val;

    HSL_DEV_ID_CHECK(dev_id);

    if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU)) {
        return SW_BAD_PARAM;
    }

    if (A_TRUE == enable) {
        val = 1;
    } else if (A_FALSE == enable) {
        val = 0;
    } else {
        return SW_BAD_PARAM;
    }

    HSL_REG_FIELD_SET(rv, dev_id, PRI_CTL, port_id,
                      EG_MAC_BASE_VLAN_EN, (a_uint8_t *) (&val), sizeof (a_uint32_t));
    return rv;

}

static sw_error_t
_isis_port_mac_vlan_xlt_get(a_uint32_t dev_id, fal_port_t port_id,
                        a_bool_t * enable)
{
    sw_error_t rv;
    a_uint32_t val;

    HSL_DEV_ID_CHECK(dev_id);

    if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU)) {
        return SW_BAD_PARAM;
    }

    HSL_REG_FIELD_GET(rv, dev_id, PRI_CTL, port_id,
                      EG_MAC_BASE_VLAN_EN, (a_uint8_t *) (&val), sizeof (a_uint32_t));
    SW_RTN_ON_ERROR(rv);

    if (1 == val) {
        *enable = A_TRUE;
    } else {
        *enable = A_FALSE;
    }

    return SW_OK;
}

/**
 * @brief Set 802.1q work mode on a particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[in] port_1qmode 802.1q work mode
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isis_port_1qmode_set(a_uint32_t dev_id, fal_port_t port_id,
                     fal_pt_1qmode_t port_1qmode)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isis_port_1qmode_set(dev_id, port_id, port_1qmode);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get 802.1q work mode on a particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[out] port_1qmode 802.1q work mode
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isis_port_1qmode_get(a_uint32_t dev_id, fal_port_t port_id,
                     fal_pt_1qmode_t * pport_1qmode)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isis_port_1qmode_get(dev_id, port_id, pport_1qmode);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Set packets transmitted out vlan tagged mode on a particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[in] port_egvlanmode packets transmitted out vlan tagged mode
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isis_port_egvlanmode_set(a_uint32_t dev_id, fal_port_t port_id,
                         fal_pt_1q_egmode_t port_egvlanmode)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isis_port_egvlanmode_set(dev_id, port_id, port_egvlanmode);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get packets transmitted out vlan tagged mode on a particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[out] port_egvlanmode packets transmitted out vlan tagged mode
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isis_port_egvlanmode_get(a_uint32_t dev_id, fal_port_t port_id,
                         fal_pt_1q_egmode_t * pport_egvlanmode)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isis_port_egvlanmode_get(dev_id, port_id, pport_egvlanmode);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Add member of port based vlan on a particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[in] mem_port_id port member
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isis_portvlan_member_add(a_uint32_t dev_id, fal_port_t port_id,
                         a_uint32_t mem_port_id)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isis_portvlan_member_add(dev_id, port_id, mem_port_id);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Delete member of port based vlan on a particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[in] mem_port_id port member
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isis_portvlan_member_del(a_uint32_t dev_id, fal_port_t port_id,
                         a_uint32_t mem_port_id)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isis_portvlan_member_del(dev_id, port_id, mem_port_id);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Update member of port based vlan on a particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[in] mem_port_map port members
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isis_portvlan_member_update(a_uint32_t dev_id, fal_port_t port_id,
                            fal_pbmp_t mem_port_map)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isis_portvlan_member_update(dev_id, port_id, mem_port_map);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get member of port based vlan on a particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[out] mem_port_map port members
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isis_portvlan_member_get(a_uint32_t dev_id, fal_port_t port_id,
                         fal_pbmp_t * mem_port_map)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isis_portvlan_member_get(dev_id, port_id, mem_port_map);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Set force default vlan id status on a particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[in] enable A_TRUE or A_FALSE
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isis_port_force_default_vid_set(a_uint32_t dev_id, fal_port_t port_id,
                                a_bool_t enable)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isis_port_force_default_vid_set(dev_id, port_id, enable);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get force default vlan id status on a particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[out] enable A_TRUE or A_FALSE
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isis_port_force_default_vid_get(a_uint32_t dev_id, fal_port_t port_id,
                                a_bool_t * enable)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isis_port_force_default_vid_get(dev_id, port_id, enable);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Set force port based vlan status on a particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[in] enable A_TRUE or A_FALSE
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isis_port_force_portvlan_set(a_uint32_t dev_id, fal_port_t port_id,
                             a_bool_t enable)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isis_port_force_portvlan_set(dev_id, port_id, enable);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get force port based vlan status on a particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[out] enable A_TRUE or A_FALSE
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isis_port_force_portvlan_get(a_uint32_t dev_id, fal_port_t port_id,
                             a_bool_t * enable)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isis_port_force_portvlan_get(dev_id, port_id, enable);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Set nest vlan tpid on a particular device.
 * @param[in] dev_id device id
 * @param[in] tpid tag protocol identification
 * @return SW_OK or error code 
 */
HSL_LOCAL sw_error_t
isis_nestvlan_tpid_set(a_uint32_t dev_id, a_uint32_t tpid)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isis_nestvlan_tpid_set(dev_id, tpid);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get nest vlan tpid on a particular device.
 * @param[in] dev_id device id
 * @param[out] tpid tag protocol identification
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isis_nestvlan_tpid_get(a_uint32_t dev_id, a_uint32_t * tpid)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isis_nestvlan_tpid_get(dev_id, tpid);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Set ingress vlan mode mode on a particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[in] mode ingress vlan mode
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isis_port_invlan_mode_set(a_uint32_t dev_id, fal_port_t port_id,
                          fal_pt_invlan_mode_t mode)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isis_port_invlan_mode_set(dev_id, port_id, mode);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get ingress vlan mode mode on a particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[out] mode ingress vlan mode
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isis_port_invlan_mode_get(a_uint32_t dev_id, fal_port_t port_id,
                          fal_pt_invlan_mode_t * mode)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isis_port_invlan_mode_get(dev_id, port_id, mode);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Set tls status on a particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[in] enable A_TRUE or A_FALSE
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isis_port_tls_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isis_port_tls_set(dev_id, port_id, enable);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get tls status on a particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[out] enable A_TRUE or A_FALSE
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isis_port_tls_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isis_port_tls_get(dev_id, port_id, enable);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Set priority propagation status on a particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[in] enable A_TRUE or A_FALSE
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isis_port_pri_propagation_set(a_uint32_t dev_id, fal_port_t port_id,
                              a_bool_t enable)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isis_port_pri_propagation_set(dev_id, port_id, enable);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get priority propagation status on a particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[out] enable A_TRUE or A_FALSE
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isis_port_pri_propagation_get(a_uint32_t dev_id, fal_port_t port_id,
                              a_bool_t * enable)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isis_port_pri_propagation_get(dev_id, port_id, enable);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Set default s-vid on a particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[in] vid s-vid
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isis_port_default_svid_set(a_uint32_t dev_id, fal_port_t port_id,
                           a_uint32_t vid)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isis_port_default_svid_set(dev_id, port_id, vid);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get default s-vid on a particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[out] vid s-vid
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isis_port_default_svid_get(a_uint32_t dev_id, fal_port_t port_id,
                           a_uint32_t * vid)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isis_port_default_svid_get(dev_id, port_id, vid);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Set default c-vid on a particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[in] vid c-vid
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isis_port_default_cvid_set(a_uint32_t dev_id, fal_port_t port_id,
                           a_uint32_t vid)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isis_port_default_cvid_set(dev_id, port_id, vid);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get default c-vid on a particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[out] vid c-vid
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isis_port_default_cvid_get(a_uint32_t dev_id, fal_port_t port_id,
                           a_uint32_t * vid)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isis_port_default_cvid_get(dev_id, port_id, vid);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Set vlan propagation status on a particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[in] mode vlan propagation mode
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isis_port_vlan_propagation_set(a_uint32_t dev_id, fal_port_t port_id,
                               fal_vlan_propagation_mode_t mode)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isis_port_vlan_propagation_set(dev_id, port_id, mode);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get vlan propagation status on a particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[out] mode vlan propagation mode
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isis_port_vlan_propagation_get(a_uint32_t dev_id, fal_port_t port_id,
                               fal_vlan_propagation_mode_t * mode)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isis_port_vlan_propagation_get(dev_id, port_id, mode);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Add a vlan translation entry to a particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param entry vlan translation entry
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isis_port_vlan_trans_add(a_uint32_t dev_id, fal_port_t port_id,
                         fal_vlan_trans_entry_t * entry)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isis_port_vlan_trans_add(dev_id, port_id, entry);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Delete a vlan translation entry from a particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param entry vlan translation entry
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isis_port_vlan_trans_del(a_uint32_t dev_id, fal_port_t port_id,
                         fal_vlan_trans_entry_t * entry)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isis_port_vlan_trans_del(dev_id, port_id, entry);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get a vlan translation entry from a particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param entry vlan translation entry
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isis_port_vlan_trans_get(a_uint32_t dev_id, fal_port_t port_id,
                         fal_vlan_trans_entry_t * entry)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isis_port_vlan_trans_get(dev_id, port_id, entry);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Iterate all vlan translation entries from a particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[in] iterator translation entry index if it's zero means get the first entry
 * @param[out] iterator next valid translation entry index
 * @param[out] entry vlan translation entry
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isis_port_vlan_trans_iterate(a_uint32_t dev_id, fal_port_t port_id,
                             a_uint32_t * iterator,
                             fal_vlan_trans_entry_t * entry)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isis_port_vlan_trans_iterate(dev_id, port_id, iterator, entry);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Set switch qinq work mode on a particular device.
 * @param[in] dev_id device id
 * @param[in] mode qinq work mode
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isis_qinq_mode_set(a_uint32_t dev_id, fal_qinq_mode_t mode)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isis_qinq_mode_set(dev_id, mode);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get switch qinq work mode on a particular device.
 * @param[in] dev_id device id
 * @param[out] mode qinq work mode
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isis_qinq_mode_get(a_uint32_t dev_id, fal_qinq_mode_t * mode)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isis_qinq_mode_get(dev_id, mode);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Set qinq role on a particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[in] role port role
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isis_port_qinq_role_set(a_uint32_t dev_id, fal_port_t port_id,
                        fal_qinq_port_role_t role)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isis_port_qinq_role_set(dev_id, port_id, role);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get qinq role on a particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[out] role port role
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isis_port_qinq_role_get(a_uint32_t dev_id, fal_port_t port_id,
                        fal_qinq_port_role_t * role)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isis_port_qinq_role_get(dev_id, port_id, role);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Set MAC_VLAN_XLT status on a particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[in] role port role
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isis_port_mac_vlan_xlt_set(a_uint32_t dev_id, fal_port_t port_id,
                        a_bool_t enable)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isis_port_mac_vlan_xlt_set(dev_id, port_id, enable);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get MAC_VLAN_XLT status on a particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[out] role port role
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isis_port_mac_vlan_xlt_get(a_uint32_t dev_id, fal_port_t port_id,
                        a_bool_t * enable)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isis_port_mac_vlan_xlt_get(dev_id, port_id, enable);
    HSL_API_UNLOCK;
    return rv;
}

sw_error_t
isis_portvlan_init(a_uint32_t dev_id)
{
    a_uint32_t i;
    sw_error_t rv;
    fal_vlan_trans_entry_t entry_init;

    HSL_DEV_ID_CHECK(dev_id);

    aos_mem_set(&entry_init, 0, sizeof (fal_vlan_trans_entry_t));

    for (i = 0; i < ISIS_MAX_VLAN_TRANS; i++) {
        rv = _isis_vlan_trans_write(dev_id, i, 0, entry_init);
        SW_RTN_ON_ERROR(rv);
    }

#ifndef HSL_STANDALONG
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    p_api->port_1qmode_get = isis_port_1qmode_get;
    p_api->port_1qmode_set = isis_port_1qmode_set;
    p_api->port_egvlanmode_get = isis_port_egvlanmode_get;
    p_api->port_egvlanmode_set = isis_port_egvlanmode_set;
    p_api->portvlan_member_add = isis_portvlan_member_add;
    p_api->portvlan_member_del = isis_portvlan_member_del;
    p_api->portvlan_member_update = isis_portvlan_member_update;
    p_api->portvlan_member_get = isis_portvlan_member_get;
    p_api->port_force_default_vid_set = isis_port_force_default_vid_set;
    p_api->port_force_default_vid_get = isis_port_force_default_vid_get;
    p_api->port_force_portvlan_set = isis_port_force_portvlan_set;
    p_api->port_force_portvlan_get = isis_port_force_portvlan_get;
    p_api->nestvlan_tpid_set = isis_nestvlan_tpid_set;
    p_api->nestvlan_tpid_get = isis_nestvlan_tpid_get;
    p_api->port_invlan_mode_set = isis_port_invlan_mode_set;
    p_api->port_invlan_mode_get = isis_port_invlan_mode_get;
    p_api->port_tls_set = isis_port_tls_set;
    p_api->port_tls_get = isis_port_tls_get;
    p_api->port_pri_propagation_set = isis_port_pri_propagation_set;
    p_api->port_pri_propagation_get = isis_port_pri_propagation_get;
    p_api->port_default_svid_set = isis_port_default_svid_set;
    p_api->port_default_svid_get = isis_port_default_svid_get;
    p_api->port_default_cvid_set = isis_port_default_cvid_set;
    p_api->port_default_cvid_get = isis_port_default_cvid_get;
    p_api->port_vlan_propagation_set = isis_port_vlan_propagation_set;
    p_api->port_vlan_propagation_get = isis_port_vlan_propagation_get;
    p_api->port_vlan_trans_add = isis_port_vlan_trans_add;
    p_api->port_vlan_trans_del = isis_port_vlan_trans_del;
    p_api->port_vlan_trans_get = isis_port_vlan_trans_get;
    p_api->qinq_mode_set = isis_qinq_mode_set;
    p_api->qinq_mode_get = isis_qinq_mode_get;
    p_api->port_qinq_role_set = isis_port_qinq_role_set;
    p_api->port_qinq_role_get = isis_port_qinq_role_get;
    p_api->port_vlan_trans_iterate = isis_port_vlan_trans_iterate;
    p_api->port_mac_vlan_xlt_set = isis_port_mac_vlan_xlt_set;
    p_api->port_mac_vlan_xlt_get = isis_port_mac_vlan_xlt_get;    
#endif

    return SW_OK;
}

/**
 * @}
 */
