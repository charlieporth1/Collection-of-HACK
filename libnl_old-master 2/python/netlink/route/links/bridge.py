#
# Copyright (c) 2011 Thomas Graf <tgraf@suug.ch>
#

"""VLAN network link

"""

from __future__ import absolute_import


from ... import core as netlink
from ..  import capi as capi

# Bridge flags
# RTNL_BRIDGE_HAIRPIN_MODE = 0x0001,
# RTNL_BRIDGE_BPDU_GUARD = 0x0002,
# RTNL_BRIDGE_ROOT_BLOCK = 0x0004,
# RTNL_BRIDGE_FAST_LEAVE = 0x0008 


class BridgeLink(object):
    def __init__(self, link):
        
        if not capi.rtnl_link_is_bridge(link):
            raise ValueError("Passed link must be a bridge")

        self._link = link


    @property
    @netlink.nlattr(type=str)
    def flags(self):
        """ Bridge flags
        Setting this property will *Not* reset flags to value you supply in
        Examples:
        link.flags = '+xxx' # add xxx flag
        link.flags = 'xxx'  # exactly the same
        link.flags = '-xxx' # remove xxx flag
        link.flags = [ '+xxx', '-yyy' ] # list operation
        """
        flags = capi.rtnl_link_bridge_get_flags(self._link)
        # TODO split flags into strings
        return 
        # capi.rtnl_link_vlan_flags2str(flags, 256)[0].split(',')

    def _set_flag(self, flag):
        if flag.startswith('-'):
            i = capi.rtnl_link_vlan_str2flags(flag[1:])
            capi.rtnl_link_vlan_unset_flags(self._link, i)
        elif flag.startswith('+'):
            i = capi.rtnl_link_vlan_str2flags(flag[1:])
            capi.rtnl_link_vlan_set_flags(self._link, i)
        else:
            i = capi.rtnl_link_vlan_str2flags(flag)
            capi.rtnl_link_vlan_set_flags(self._link, i)

    @flags.setter
    def flags(self, value):
        if type(value) is list:
            for flag in value:
                self._set_flag(flag)
        else:
            self._set_flag(value)

    ###################################################################
    # TODO:
    #   - ingress map
    #   - egress map

    def brief(self):
        return 'bridge-id {0}'.format(self.id)

def init(link):
    return BridgeLink(link._rtnl_link)
    # return link.vlan
