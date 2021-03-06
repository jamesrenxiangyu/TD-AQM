# -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-

# def options(opt):
#     pass

# def configure(conf):
#     conf.check_nonfatal(header_name='stdint.h', define_name='HAVE_STDINT_H')

def build(bld):
    module = bld.create_ns3_module('TD-AQM', ['core'])
    module.source = [
        'model/ipv4-dsr-routing-table-entry.cc',
        'model/dsr-header.cc',
        'model/ipv4-dsr-routing.cc',
        'model/dsr-router-interface.cc',
        'model/dsr-route-manager.cc',
        'model/dsr-route-manager-impl.cc',
        'model/dsr-candidate-queue.cc',
        'model/dsr-application.cc',
        'model/dsr-sink.cc',
        'model/dsr-virtual-queue-disc.cc',
        'helper/ipv4-dsr-routing-helper.cc',
        'helper/dsr-application-helper.cc',
        'helper/dsr-sink-helper.cc',
        ]

    module_test = bld.create_ns3_module_test_library('TD-AQM')
    module_test.source = [
        # 'test/dsr-routing-test-suite.cc',
        # 'test/test-dsr-header.cc',
        ]
    # Tests encapsulating example programs should be listed here
    if (bld.env['ENABLE_EXAMPLES']):
        module_test.source.extend([
        #    'test/dsr-routing-examples-test-suite.cc',
             ])

    headers = bld(features='ns3header')
    headers.module = 'TD-AQM'
    headers.source = [
        'model/ipv4-dsr-routing-table-entry.h',
        'model/dsr-header.h',
        'model/ipv4-dsr-routing.h',
        'model/dsr-router-interface.h',
        'model/dsr-route-manager.h',
        'model/dsr-route-manager-impl.h',
        'model/dsr-candidate-queue.h',
        'model/dsr-application.h',
        'model/dsr-sink.h',
        'model/dsr-virtual-queue-disc.h',
        'helper/ipv4-dsr-routing-helper.h',
        'helper/dsr-application-helper.h',
        'helper/dsr-sink-helper.h',
        ]

    if bld.env.ENABLE_EXAMPLES:
        bld.recurse('examples')

    # bld.ns3_python_bindings()

