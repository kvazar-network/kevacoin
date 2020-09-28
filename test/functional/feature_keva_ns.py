#!/usr/bin/env python3
# Copyright (c) 2016-2017 The Bitcoin Core developers
# Copyright (c) 2018-2019 The Kevacoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the Key-Value Store."""

from test_framework.blocktools import witness_script, send_to_witness
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *
from test_framework.authproxy import JSONRPCException
from io import BytesIO
from string import Template
import decimal
import re
import json

class DecimalEncoder(json.JSONEncoder):
    def default(self, o):
        if isinstance(o, decimal.Decimal):
            return float(o)
        return super(DecimalEncoder, self).default(o)

class KevaTest(BitcoinTestFramework):

    def sync_generate(self, node, num):
        self.sync_all()
        node.generate(num)
        self.sync_all()

    def set_test_params(self):
        self.num_nodes = 3
        self.setup_clean_chain = True

    def setup_network(self):
        self.setup_nodes()
        connect_nodes(self.nodes[0], 1)
        connect_nodes(self.nodes[1], 0)
        connect_nodes(self.nodes[2], 0)
        self.sync_all()

    def dumpTx(self, tx):
        print(json.dumps(self.nodes[0].decoderawtransaction(self.nodes[0].getrawtransaction(tx)), indent=4, cls=DecimalEncoder))

    def run_test(self):
        self.nodes[0].generate(2)
        hash = self.nodes[0].getbestblockhash()
        # THRESHOLD_DEFINED
        assert(self.nodes[0].getblock(hash)['versionHex'] == '20000000')

        self.nodes[0].generate(150)
        hash = self.nodes[0].getbestblockhash()
        # THRESHOLD_STARTED
        assert(self.nodes[0].getblock(hash)['versionHex'] == '20000008')

        self.nodes[0].generate(150)
        hash = self.nodes[0].getbestblockhash()
        # THRESHOLD_LOCKED_IN
        assert(self.nodes[0].getblock(hash)['versionHex'] == '20000008')

        self.nodes[0].generate(150)
        hash = self.nodes[0].getbestblockhash()
        # THRESHOLD_ACITVE
        assert(self.nodes[0].getblock(hash)['versionHex'] == '20000000')

        addr1 = self.nodes[1].getnewaddress()
        addr2 = self.nodes[2].getnewaddress()
        amount = Template('{"$addr1":1,"$addr2":1}').substitute(addr1=addr1, addr2=addr2)
        amountJson = json.loads(amount)
        self.nodes[0].sendmany("", amountJson)
        self.sync_generate(self.nodes[0], 1)
        ns1 = (self.nodes[1].keva_namespace('NS 1'))["namespaceId"]
        ns2 = (self.nodes[2].keva_namespace('NS 2'))["namespaceId"]
        self.sync_generate(self.nodes[0], 1)
        # Since nsfix is enabled, the namespace will be the different.
        assert(ns1 != ns2)

        # The namespace should be created.
        assert((self.nodes[0].keva_filter(ns1))[0]["value"] == 'NS 1')
        assert((self.nodes[0].keva_filter(ns2))[0]["value"] == 'NS 2')

        self.nodes[1].keva_put(ns1, 'key', 'value 1')
        self.nodes[2].keva_put(ns2, 'key', 'value 2')
        self.sync_generate(self.nodes[0], 1)
        assert((self.nodes[0].keva_get(ns1, 'key'))["value"] == 'value 1')
        assert((self.nodes[0].keva_get(ns2, 'key'))["value"] == 'value 2')

        self.nodes[0].generate(20)
        ns3 = (self.nodes[1].keva_namespace('NS 3'))["namespaceId"]
        ns4 = (self.nodes[2].keva_namespace('NS 4'))["namespaceId"]
        assert(ns3 != ns4)

        # The namespace should be created.
        self.sync_generate(self.nodes[0], 1)
        assert((self.nodes[0].keva_filter(ns3))[0]["value"] == 'NS 3')
        assert((self.nodes[0].keva_filter(ns4))[0]["value"] == 'NS 4')

        # Transfer namespace "NS 3" from node 1 to node 2.
        assert(len(self.nodes[1].keva_list_namespaces()) == 2)
        addrNode2 = self.nodes[2].getnewaddress()
        self.nodes[1].keva_put(ns3, 'key', 'vaue 1', addrNode2)
        assert(len(self.nodes[1].keva_list_namespaces()) == 1)
        assert(self.nodes[1].keva_list_namespaces()[0]['displayName'] == 'NS 1')

        # Transfer namespace "NS 1" from node 1 to node 2.
        addrNode22 = self.nodes[2].getnewaddress()
        self.nodes[1].keva_put(ns1, 'key', 'vaue 1', addrNode22)
        assert(len(self.nodes[1].keva_list_namespaces()) == 0)

        # Node 2 should have 4 namespaces.
        self.sync_generate(self.nodes[0], 1)
        assert(len(self.nodes[2].keva_list_namespaces()) == 4)


if __name__ == '__main__':
    KevaTest().main()
