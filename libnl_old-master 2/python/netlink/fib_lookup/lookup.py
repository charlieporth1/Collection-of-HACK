from __future__ import absolute_import

from .. import core as nl
from .. import capi as core_capi
from .  import capi as capi_lookup
from .. import util as util
from netlink.route import route as nlrtr
# from netlink.route.route import * as nlrtr

# import netlink.route.route as nlrtr

class Result(nl.Object):
    def __init__(self,obj=None):
        print("new results")
        super().__init__("fib_lookup/result","result", obj)


    def _obj2type(self,obj):
        return capi_lookup.obj2result(obj)

#     "fib_lookup/result"
#     "fib_lookup/fib_lookup"
# send request
# int flnl_lookup     (   struct nl_sock *    sk,
#         struct flnl_request *   req,
#         struct nl_cache *   cache 
#     )   
#  can be cached or so it seems


class Request(nl.Object):

    def __init__(self,obj=None):

        self._nl_req = capi_lookup.flnl_request_alloc()
        # Crashes
        # super().__init__("lookup/request","request", obj)
        # print("plop")
        # super().__init__("fib_lookup/request","request", obj)
        # super().__init__("route/route","request", obj)
        # print("parent init ")
        # self._nl_req = self._obj2type(self._nl_object)

    def __del__(self):
        pass

    def format(self):

    # def __str__(self):
        return "This is a request"

    @property
    def table(self):
        return capi_lookup.flnl_request_get_table(self._nl_req)

    """ Expects a table """
    @table.setter
    def table(self, tableId):
        rt = nlrtr.RoutingTable(tableId)
        # if isinstance(table, nlrtr.RoutingTable):
        # if isinstance(tableId, int):
        #     # pass #TODO convert
        #     tableId=table
        # if isinstance(table, str):
        #     pass

        capi_lookup.flnl_request_set_table( self._nl_req, rt.id )
        # return False

    @property
    def scope(self):
        return capi_lookup.flnl_request_get_scope(self._nl_req)

    @scope.setter
    def scope(self, value ):
        capi_lookup.flnl_request_set_scope(self._nl_req, int(value) )


    """ """
    @property
    def address(self):
        return capi_lookup.flnl_request_get_addr( self._nl_req )

    @address.setter
    def address(self,addr):
        capi_lookup.flnl_request_set_addr(self._nl_req, addr._nl_addr)

    @staticmethod
    def _obj2type(obj):
        return capi_lookup.obj2request(obj)



class FIB_Cache(nl.Cache):

    def __init__(self, cache=None):


        if not cache:
            cache = self._alloc_cache_name('fib_lookup/fib_lookup')

        super().__init__(nl.NETLINK_FIB_LOOKUP, cache)


        # self._protocol = nl.NETLINK_ROUTE
        # self._nl_cache = cache
        
    #override
    # def lookup(sk, addr,table, ):
    def lookup(self, request, sk=None):

        if not isinstance(request, Request):
            raise ValueError("Expects a request")

        # address = nl.AbstractAddress("8.8.8.8")
        if not sk:
            sk = nl.lookup_socket( self._protocol)

        ret = capi_lookup.flnl_lookup( sk._sock, request._nl_req, self._nl_cache);
        return ret

    # implemented by sub classes, must return new instasnce of cacheable
    # object
    @staticmethod
    def _new_object(obj):
        return Result(obj)

    # implemented by sub classes, must return instance of sub class
    def _new_cache(self, cache):
        return FIB_Cache( self._nl_cache )





if __name__ == '__main__':
    # TODO add some tests
    pass