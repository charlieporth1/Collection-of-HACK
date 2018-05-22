%module capi
%{
/*
#include <netlink/route/rtnl.h>
#include <netlink/route/link.h>
#include <netlink/route/link/vlan.h>
#include <netlink/route/link/macvlan.h>
#include <netlink/route/link/vxlan.h>
#include <netlink/route/link/inet.h>
#include <netlink/route/link/bridge.h>

#include <netlink/route/tc.h>
#include <netlink/route/qdisc.h>
#include <netlink/route/class.h>
#include <netlink/route/classifier.h>

#include <netlink/route/qdisc/htb.h>

#include <netlink/route/addr.h>
#include <netlink/route/route.h>
#include <netlink/route/rule.h>
*/

#include <netlink/fib_lookup/request.h>
#include <netlink/fib_lookup/lookup.h>
%}

%include <stdint.i>
%include <cstring.i>



/* <netlink/fib_lookup/request.h> */

%inline %{
        struct flnl_request *obj2request(struct nl_object *obj)
        {
                return (struct flnl_request *) obj;
        }
%};


extern struct flnl_request * flnl_request_alloc (void);
extern void flnl_request_set_table (struct flnl_request *req, int table);

extern void flnl_request_set_scope (struct flnl_request *req, int scope);
extern int flnl_request_get_scope (struct flnl_request *req);

extern int flnl_request_set_addr (struct flnl_request *req, struct nl_addr *addr);


// this is defined in lookup.c
// extern void flnl_result_put(struct flnl_result *);


/* <netlink/fib_lookup/lookup.h> */
//struct nl_cache *flnl_result_alloc_cache(void)


%inline %{
        struct nl_object *result2obj(struct flnl_result *addr)
        {
                return OBJ_CAST(addr);
        }

        struct flnl_result *obj2result(struct nl_object *obj)
        {
                return (struct flnl_result *) obj;
        }
%};


extern int flnl_lookup(struct nl_sock *, struct flnl_request *,
                                            struct nl_cache *);
