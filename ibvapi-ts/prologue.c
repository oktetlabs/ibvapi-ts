/* SPDX-License-Identifier: Apache-2.0
 * Copyright (C) 2012-2022 OKTET Labs Ltd.
 */
/** @file
 * @brief InfiniBand Verbs API Test Suite
 *
 * Test Suite prologue.
 *
 * @author Yurij Plotnikov <Yurij.Plotnikov@oktetlabs.ru>
 */

#ifndef DOXYGEN_TEST_SPEC

/** Logging subsystem entity name */
#define TE_TEST_NAME    "prologue"

#include "ibvapi-test.h"

#if HAVE_NET_ETHERNET_H
#include <net/ethernet.h>
#endif

#include "tapi_cfg_base.h"
#include "tapi_cfg_net.h"
#include "logger_ten.h"
#include "tapi_test.h"
#include "tapi_cfg.h"

/** FTL script to set method */
#define AUX_FTL \
"set printf io.fprintf io.out"

/** FTL script to retrieve some fields of LLAP MIB */
#define LLAP_FTL \
"set gllap[]:{ \
  forall (llap.mib!) [row]:{ printf \"%[llap]v %[up]v \" row!; \
    if (inenv row \"nic\"!) { \
        printf \"%[nic]v %[port]v %[mac]v %[mtu]v %[encap]v %[vlan]v\\n\" row! \
    }{ \
        printf \"* * *                 *    * *\\n\" <>! \
    }! \
  }! \
}"

/** FTL script to retrieve some fields of IPIF MIB */
#define IPIF_FTL \
"set gipif[]:{ forall (ipif.mib!) [row]:{io.write io.out \
\"\"+(str row.llap!)+\" \"+(str row.ip!)+\" \"+(str row.mask!)+\" \
\"+(str row.bcast!)+\"\\n\"! }!; }"

/** FTL script to retrieve some fields of MAC MIB */
#define MAC_FTL \
"set gmac[]:{ forall (mac.mib!) [row]:{io.write io.out \
\"\"+(str row.ip!)+\" \"+(str row.mac!)+\"\\n\"! }!; }"
/** FTL script to retrieve some fields of FWD MIB */
#define FWD_FTL "set gfwd[]:{ forall (fwd.mib!) [row]:{io.write io.out \
\"\"+(str row.ipmatch!)+\" \"+(str row.ipmask!)+\" \"\
+(str row.ip!)+\"\\n\"! }!; }"


/**
 * Copy InfiniBand Verbs API libraries specified in /local/ibvlib instances
 * to corresponding test agent.
 *
 * @return Status code.
 */
static te_errno
copy_ibvlibs(void)
{
    te_errno        rc;
    unsigned int    n_ibvlibs;
    cfg_handle     *ibvlibs = NULL;
    unsigned int    i;
    cfg_val_type    val_type;
    cfg_oid        *oid = NULL;

    rc = cfg_find_pattern("/local:*/ibvlib:", &n_ibvlibs, &ibvlibs);
    if (rc != 0)
    {
        TEST_FAIL("cfg_find_pattern(/local:*/ibvlib:) failed: %r", rc);
    }
    for (i = 0; i < n_ibvlibs; ++i)
    {
        const char * const libdir_def = "/usr/lib";
        const char * const remote_libname = "libte-iut.so";

        char   *ibvlib;
        char   *libdir;
        char   *lcplane = NULL;
        char   *remote_file;

        val_type = CVT_STRING;
        rc = cfg_get_instance(ibvlibs[i], &val_type, &ibvlib);
        if (rc != 0)
        {
            free(ibvlibs);
            TEST_FAIL("cfg_get_instance() failed: %r", rc);
        }
        if (strlen(ibvlib) == 0)
        {
            free(ibvlib);
            rc = cfg_del_instance(ibvlibs[i], FALSE);
            if (rc != 0)
            {
                TEST_FAIL("cfg_del_instance() failed: %r", rc);
            }
            continue;
        }

        rc = cfg_get_oid(ibvlibs[i], &oid);
        if (rc != 0)
        {
            free(ibvlib);
            free(ibvlibs);
            TEST_FAIL("cfg_get_oid() failed: %r", rc);
        }

        val_type = CVT_STRING;
        rc = cfg_get_instance_fmt(&val_type, &libdir,
                                  "/local:%s/libdir:",
                                  CFG_OID_GET_INST_NAME(oid, 1));
        if (rc == TE_RC(TE_CS, TE_ENOENT))
        {
            libdir = (char *)libdir_def;
        }
        else if (rc != 0)
        {
            cfg_free_oid(oid);
            free(ibvlib);
            free(ibvlibs);
            TEST_FAIL("%u: cfg_get_instance_fmt() failed", __LINE__);
        }

        /* Prepare name of the remote file to put */
        remote_file = malloc(strlen(libdir) + 1 /* '/' */ +
                             strlen(remote_libname) + 1 /* '\0' */);
        if (remote_file == NULL)
        {
            if (libdir != libdir_def)
                free(libdir);
            cfg_free_oid(oid);
            free(ibvlib);
            free(ibvlibs);
            TEST_FAIL("Memory allocation failure");
        }

        strcpy(remote_file, libdir);
        if (libdir != libdir_def)
            free(libdir);
        strcat(remote_file, "/");
        strcat(remote_file, remote_libname);

        rc = rcf_ta_put_file(CFG_OID_GET_INST_NAME(oid, 1), 0,
                             ibvlib, remote_file);
        if (rc != 0)
        {
            ERROR("Failed to put file '%s' to %s:%s",
                  ibvlib, CFG_OID_GET_INST_NAME(oid, 1), remote_file);
            free(remote_file);
            cfg_free_oid(oid);
            free(ibvlib);
            free(ibvlibs);
            goto cleanup;
        }
        RING("File '%s' put to %s:%s", ibvlib,
             CFG_OID_GET_INST_NAME(oid, 1), remote_file);

        val_type = CVT_STRING;
        rc = cfg_get_instance_fmt(&val_type, &lcplane,
                                  "/local:%s/lcplane:",
                                  CFG_OID_GET_INST_NAME(oid, 1));

        free(ibvlib);
        rc = cfg_set_instance(ibvlibs[i], val_type, remote_file);
        if (rc != 0)
        {
            free(remote_file);
            cfg_free_oid(oid);
            free(ibvlibs);
            TEST_FAIL("cfg_set_instance() failed: %r", rc);
        }

        {
            int rc2;

            rc = rcf_ta_call(CFG_OID_GET_INST_NAME(oid, 1), 0, "shell",
                             &rc2, 3, TRUE, "chmod", "+s", remote_file);
            if (rc != 0)
            {
                ERROR("Failed to call 'shell' on %s: %r",
                      CFG_OID_GET_INST_NAME(oid, 1), rc);
                free(remote_file);
                cfg_free_oid(oid);
                free(ibvlibs);
                goto cleanup;
            }
            if ((rc = rc2) != 0)
            {
                ERROR("Failed to execute 'chmod' on %s: %r",
                      CFG_OID_GET_INST_NAME(oid, 1), rc2);
                free(remote_file);
                cfg_free_oid(oid);
                free(ibvlibs);
                goto cleanup;
            }
        }
        free(remote_file);
        cfg_free_oid(oid);
    }
    free(ibvlibs);

cleanup:
    return rc;
}

/**
 * Start background corruption engine on the IUT.
 *
 * @param pco_iut       PCO to be used for corruption engine starting
 */
static void
start_corruption_engine(rcf_rpc_server *pco_iut)
{
    char *script = getenv("TE_CORRUPTION_SCRIPT");

    tarpc_pid_t pid = -1;
    tarpc_uid_t uid = 0;

    if (script == NULL || *script == 0)
        return;

    pid = rpc_te_shell_cmd(pco_iut, script, uid, NULL, NULL, NULL);

    if (cfg_add_instance_str("/local:/corruption_pid:", NULL,
                             CFG_VAL(INTEGER, pid)) != 0)
    {
        rpc_ta_kill_death(pco_iut, pid);
        TEST_FAIL("Failed to store corruption engine PID in configurator"
                  " local tree");
    }
}

/**
 * Set InfiniBand Verbs API library names for Test Agents in accordance
 * with configuration in configurator.conf.
 *
 * @retval EXIT_SUCCESS     success
 * @retval EXIT_FAILURE     failure
 */
int
main(int argc, char **argv)
{
    unsigned int    i, j, k;
    cfg_val_type    val_type;

    cfg_nets_t      nets;

    cfg_oid        *oid = NULL;

    unsigned int    sleep_time;
    int             use_static_arp_def;
    int             use_static_arp;

    char           *st_rpcs_no_share = getenv("ST_RPCS_NO_SHARE");
    char           *st_no_ip6 = getenv("ST_NO_IP6");

    rcf_rpc_server *pco_iut;

/* Redefine as empty to avoid environment processing here */
#undef TEST_START_SPECIFIC
#define TEST_START_SPECIFIC
    TEST_START;
    tapi_env_init(&env);

    rc = tapi_cfg_env_local_to_agent();
    if (rc != 0)
    {
        TEST_FAIL("tapi_cfg_env_local_to_agent() failed: %r", rc);
    }

    if ((rc = tapi_cfg_net_remove_empty()) != 0)
        TEST_FAIL("Failed to remove /net instances with empty interfaces");

    rc = tapi_cfg_net_reserve_all();
    if (rc != 0)
    {
        TEST_FAIL("Failed to reserve all interfaces mentioned in networks "
                  "configuration: %r", rc);
    }

    rc = tapi_cfg_net_all_up(FALSE);
    if (rc != 0)
    {
        TEST_FAIL("Failed to up all interfaces mentioned in networks "
                  "configuration: %r", rc);
    }

    rc = tapi_cfg_net_delete_all_ip4_addresses();
    if (rc != 0)
    {
        TEST_FAIL("Failed to delete all IPv4 addresses from all "
                  "interfaces mentioned in networks configuration: %r",
                  rc);
    }

    /* Get default value for 'use_static_arp' */
    val_type = CVT_INTEGER;
    rc = cfg_get_instance_fmt(&val_type, &use_static_arp_def,
                              "/local:/use_static_arp:");
    if (rc != 0)
    {
        use_static_arp_def = 0;
        WARN("Failed to get /local:/use_static_arp: default value, "
             "set to %d", use_static_arp_def);
    }

    /* Get available networks configuration */
    rc = tapi_cfg_net_get_nets(&nets);
    if (rc != 0)
    {
        TEST_FAIL("Failed to get networks from Configurator: %r", rc);
    }

    for (i = 0; i < nets.n_nets; ++i)
    {
        cfg_net_t  *net = nets.nets + i;


        rc = tapi_cfg_net_assign_ip(AF_INET, net, NULL);
        if (rc != 0)
        {
            ERROR("Failed to assign IPv4 subnet to net #%u: %r", i, rc);
            break;
        }

        /* Add static ARP entires */
        for (j = 0; j < net->n_nodes; ++j)
        {
            char               *node_oid = NULL;
            char               *if_oid = NULL;
            unsigned int        ip4_addrs_num;
            cfg_handle         *ip4_addrs;
            struct sockaddr    *ip4_addr = NULL;
            uint8_t             mac[ETHER_ADDR_LEN];

            /* Get IPv4 address assigned to the node */
            rc = cfg_get_oid_str(net->nodes[j].handle, &node_oid);
            if (rc != 0)
            {
                ERROR("Failed to string OID by handle: %r", rc);
                break;
            }

            /* Get IPv4 addresses of the node */
            rc = cfg_find_pattern_fmt(&ip4_addrs_num, &ip4_addrs,
                                      "%s/ip4_address:*", node_oid);
            if (rc != 0)
            {
                ERROR("Failed to find IPv4 addresses assigned to node "
                      "'%s': %r", node_oid, rc);
                free(node_oid);
                break;
            }
            if (ip4_addrs_num == 0)
            {
                ERROR("No IPv4 addresses are assigned to node '%s'",
                      node_oid);
                free(ip4_addrs);
                free(node_oid);
                rc = TE_EENV;
                break;
            }
            val_type = CVT_ADDRESS;
            rc = cfg_get_instance(ip4_addrs[0], &val_type, &ip4_addr);
            free(ip4_addrs);
            free(node_oid);
            if (rc != 0)
            {
                ERROR("Failed to get node IPv4 address: %r", rc);
                break;
            }

            /* Get MAC address of the network interface */
            val_type = CVT_STRING;
            rc = cfg_get_instance(net->nodes[j].handle, &val_type,
                                  &if_oid);
            if (rc != 0)
            {
                ERROR("Failed to get Configurator instance by handle "
                      "0x%x: %r", net->nodes[j].handle, rc);
                free(ip4_addr);
                break;
            }
            rc = tapi_cfg_base_if_get_mac(if_oid, mac);
            if (rc != 0)
            {
                ERROR("Failed to get MAC address of %s: %r", if_oid, rc);
                free(if_oid);
                free(ip4_addr);
                break;
            }
            free(if_oid);

            /* Add static ARP entry for all other nodes */
            for (k = 0; k < net->n_nodes; ++k)
            {
                if (j == k)
                    continue;

                /* Get network node OID and agent name in it */
                val_type = CVT_STRING;
                rc = cfg_get_instance(net->nodes[k].handle, &val_type,
                                      &if_oid);
                if (rc != 0)
                {
                    ERROR("Failed to string OID by handle: %r", rc);
                    break;
                }
                oid = cfg_convert_oid_str(if_oid);
                if (oid == NULL)
                {
                    ERROR("Failed to convert OID from string '%s' to "
                          "struct", if_oid);
                    free(if_oid);
                    break;
                }
                free(if_oid);

                /* Should we use static ARP for this TA? */
                val_type = CVT_INTEGER;
                rc = cfg_get_instance_fmt(&val_type, &use_static_arp,
                                          "/local:%s/use_static_arp:",
                                          CFG_OID_GET_INST_NAME(oid, 1));
                if (TE_RC_GET_ERROR(rc) == TE_ENOENT)
                {
                    use_static_arp = use_static_arp_def;
                    rc = 0;
                }
                else if (rc != 0)
                {
                    ERROR("Failed to get /local:%s/use_static_arp: "
                          "value: %r", CFG_OID_GET_INST_NAME(oid, 1),
                          rc);
                    break;
                }
                if (!use_static_arp)
                {
                    cfg_free_oid(oid);
                    continue;
                }

                /* Add static ARP entry */
                rc = tapi_cfg_add_neigh_entry(CFG_OID_GET_INST_NAME(oid, 1),
                                              CFG_OID_GET_INST_NAME(oid, 2),
                                              ip4_addr, mac, TRUE);
                if (rc != 0)
                {
                    ERROR("Failed to add static ARP entry to TA '%s': %r",
                          CFG_OID_GET_INST_NAME(oid, 1), rc);
                    cfg_free_oid(oid);
                    break;
                }
                cfg_free_oid(oid);
            }
            free(ip4_addr);
            if (rc != 0)
                break;
        }
        if (rc != 0)
            break;

        if (st_no_ip6 == NULL || *st_no_ip6 == '\0')
        {
            rc = tapi_cfg_net_assign_ip(AF_INET6, net, NULL);
            if (rc != 0)
            {
                ERROR("Failed to assign IPv6 subnet to net #%u: %r", i, rc);
                break;
            }
        }
    }
    tapi_cfg_net_free_nets(&nets);
    if (rc != 0)
    {
        TEST_FAIL("Failed to prepare testing networks");
    }

    rc = copy_ibvlibs();
    if (rc != 0)
    {
        TEST_FAIL("Processing of /local:*/ibvlib: failed: %r", rc);
    }
    if (st_rpcs_no_share == NULL || *st_rpcs_no_share == '\0')
    {
        rc = tapi_cfg_rpcs_local_to_agent();
        if (rc != 0)
        {
            TEST_FAIL("Failed to start all RPC servers: %r", rc);
        }
    }

    val_type = CVT_INTEGER;
    rc = cfg_get_instance_fmt(&val_type, &sleep_time,
                              "/local:/prologue_sleep:");
    if (rc != 0)
    {
        WARN("Failed to get /local:/prologue_sleep: value: %r", rc);
    }
    else if (sleep_time > 0)
    {
        SLEEP(sleep_time);
    }

    CFG_WAIT_CHANGES;
    CHECK_RC(rc = cfg_synchronize("/:", TRUE));

    TEST_START_ENV;
    TEST_GET_PCO(pco_iut);
    start_corruption_engine(pco_iut);

    CHECK_RC(rc = cfg_tree_print(NULL, TE_LL_RING, "/:"));

    TEST_SUCCESS;

cleanup:

    TEST_END;
}

#endif /* !DOXYGEN_TEST_SPEC */
