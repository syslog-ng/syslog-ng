#############################################################################
# Copyright (c) 2022 Balazs Scheidler <bazsi77@gmail.com>
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 as published
# by the Free Software Foundation, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#
# As an additional exemption you are allowed to compile & link against the
# OpenSSL libraries as published by the OpenSSL project. See the file
# COPYING for details.
#
#############################################################################
from syslogng.modules.kubernetes import KubernetesAPIEnrichment
from syslogng.message import LogMessage
from syslogng.template import LogTemplate
import pytest
import json

import kubernetes.client


@pytest.fixture
def minimal_config():
    return {
        'prefix': 'myk8s.',
    }


@pytest.fixture
def mock_kube_config(mocker):
    client_configuration = kubernetes.client.Configuration()
    client_configuration.host = "https://dummy.kube.cluster:9090/"
    client_configuration.api_key = "bearer dummytoken"
    kubernetes.client.Configuration.set_default(client_configuration)

    yield mocker.patch('kubernetes.config.load_config', return_value=None)

    kubernetes.client.Configuration.set_default(None)


@pytest.fixture
def mock_api_response_nginx(mocker):
    response_json = {
        'kind': 'PodList',
        'apiVersion': 'v1',
        'metadata': {'selfLink': '/api/v1/namespaces/default/pods', 'resourceVersion': '738405'},
        'items': [
            {
                'metadata': {
                    'name': 'nginx-85b98978db-2dpdh',
                    'generateName': 'nginx-85b98978db-',
                    'namespace': 'default',
                    'selfLink': '/api/v1/namespaces/default/pods/nginx-85b98978db-2dpdh',
                    'uid': 'd7a782cf-cfaa-45e6-8f22-950b2df5bf82',
                    'resourceVersion': '738230',
                    'creationTimestamp': '2022-04-20T08:08:37Z',
                    'labels': {'app': 'nginx', 'pod-template-hash': '85b98978db'},
                    'annotations': {
                        'cni.projectcalico.org/podIP': '10.1.71.91/32',
                        'cni.projectcalico.org/podIPs': '10.1.71.91/32'
                    },
                    'ownerReferences': [
                        {
                            'apiVersion': 'apps/v1',
                            'kind': 'ReplicaSet',
                            'name': 'nginx-85b98978db',
                            'uid': '2958d7c9-f6c0-4f65-8943-0c6dd54bb98f',
                            'controller': True,
                            'blockOwnerDeletion': True
                        }
                    ],
                    'managedFields': [
                        {
                            'manager': 'kubelite',
                            'operation': 'Update',
                            'apiVersion': 'v1',
                            'time': '2022-04-20T08:08:37Z',
                            'fieldsType': 'FieldsV1',
                            'fieldsV1': {'f:metadata': {'f:generateName': {}, 'f:labels': {'.': {}, 'f:app': {}, 'f:pod-template-hash': {}}, 'f:ownerReferences': {'.': {}, 'k:{"uid":"2958d7c9-f6c0-4f65-8943-0c6dd54bb98f"}': {}}}, 'f:spec': {'f:containers': {'k:{"name":"nginx"}': {'.': {}, 'f:image': {}, 'f:imagePullPolicy': {}, 'f:name': {}, 'f:resources': {}, 'f:terminationMessagePath': {}, 'f:terminationMessagePolicy': {}}}, 'f:dnsPolicy': {}, 'f:enableServiceLinks': {}, 'f:restartPolicy': {}, 'f:schedulerName': {}, 'f:securityContext': {}, 'f:terminationGracePeriodSeconds': {}}}
                        },
                        {
                            'manager': 'calico',
                            'operation': 'Update',
                            'apiVersion': 'v1',
                            'time': '2022-04-20T08:08:38Z',
                            'fieldsType': 'FieldsV1',
                            'fieldsV1': {'f:metadata': {'f:annotations': {'.': {}, 'f:cni.projectcalico.org/podIP': {}, 'f:cni.projectcalico.org/podIPs': {}}}},
                            'subresource': 'status'
                        },
                        {
                            'manager': 'Go-http-client',
                            'operation': 'Update',
                            'apiVersion': 'v1',
                            'time': '2022-04-29T19:56:29Z',
                            'fieldsType': 'FieldsV1',
                            'fieldsV1': {'f:status': {'f:conditions': {'k:{"type":"ContainersReady"}': {'.': {}, 'f:lastProbeTime': {}, 'f:type': {}}, 'k:{"type":"Initialized"}': {'.': {}, 'f:lastProbeTime': {}, 'f:lastTransitionTime': {}, 'f:status': {}, 'f:type': {}}, 'k:{"type":"Ready"}': {'.': {}, 'f:lastProbeTime': {}, 'f:type': {}}}, 'f:phase': {}, 'f:startTime': {}}},
                            'subresource': 'status'
                        },
                        {
                            'manager': 'kubelite',
                            'operation': 'Update',
                            'apiVersion': 'v1',
                            'time': '2022-11-01T12:04:11Z',
                            'fieldsType': 'FieldsV1',
                            'fieldsV1': {'f:status': {'f:conditions': {'k:{"type":"ContainersReady"}': {'f:lastTransitionTime': {}, 'f:status': {}}, 'k:{"type":"Ready"}': {'f:lastTransitionTime': {}, 'f:status': {}}}, 'f:containerStatuses': {}, 'f:hostIP': {}, 'f:podIP': {}, 'f:podIPs': {'.': {}, 'k:{"ip":"10.1.71.91"}': {'.': {}, 'f:ip': {}}}}},
                            'subresource': 'status'
                        }
                    ]
                },
                'spec': {
                    'volumes': [
                        {
                            'name': 'kube-api-access-7xzch',
                            'projected': {
                                'sources': [
                                    {
                                        'serviceAccountToken': {
                                            'expirationSeconds': 3607,
                                            'path': 'token'}
                                    },
                                    {
                                        'configMap': {
                                            'name': 'kube-root-ca.crt',
                                            'items': [{'key': 'ca.crt', 'path': 'ca.crt'}]
                                        }
                                    },
                                    {
                                        'downwardAPI': {
                                            'items': [{'path': 'namespace', 'fieldRef': {'apiVersion': 'v1', 'fieldPath': 'metadata.namespace'}}]
                                        }
                                    }
                                ],
                                'defaultMode': 420}
                        }
                    ],
                    'containers': [
                        {
                            'name': 'nginx',
                            'image': 'nginx',
                            'resources': {},
                            'volumeMounts': [{'name': 'kube-api-access-7xzch', 'readOnly': True, 'mountPath': '/var/run/secrets/kubernetes.io/serviceaccount'}],
                            'terminationMessagePath': '/dev/termination-log',
                            'terminationMessagePolicy': 'File',
                            'imagePullPolicy': 'Always'
                        }
                    ],
                    'restartPolicy': 'Always',
                    'terminationGracePeriodSeconds': 30,
                    'dnsPolicy': 'ClusterFirst',
                    'serviceAccountName': 'default',
                    'serviceAccount': 'default',
                    'nodeName': 'bzorp',
                    'securityContext': {},
                    'schedulerName': 'default-scheduler',
                    'tolerations': [
                        {'key': 'node.kubernetes.io/not-ready', 'operator': 'Exists', 'effect': 'NoExecute', 'tolerationSeconds': 300},
                        {'key': 'node.kubernetes.io/unreachable', 'operator': 'Exists', 'effect': 'NoExecute', 'tolerationSeconds': 300}
                    ],
                    'priority': 0,
                    'enableServiceLinks': True,
                    'preemptionPolicy': 'PreemptLowerPriority'
                },
                'status': {
                    'phase': 'Running',
                    'conditions': [
                        {'type': 'Initialized', 'status': 'True', 'lastProbeTime': None, 'lastTransitionTime': '2022-04-20T08:08:37Z'},
                        {'type': 'Ready', 'status': 'True', 'lastProbeTime': None, 'lastTransitionTime': '2022-11-01T12:04:11Z'},
                        {'type': 'ContainersReady', 'status': 'True', 'lastProbeTime': None, 'lastTransitionTime': '2022-11-01T12:04:11Z'},
                        {'type': 'PodScheduled', 'status': 'True', 'lastProbeTime': None, 'lastTransitionTime': '2022-04-20T08:08:37Z'}
                    ],
                    'hostIP': '192.168.78.135',
                    'podIP': '10.1.71.91',
                    'podIPs': [{'ip': '10.1.71.91'}],
                    'startTime': '2022-04-20T08:08:37Z',
                    'containerStatuses': [
                        {
                            'name': 'nginx',
                            'state': {'running': {'startedAt': '2022-11-01T12:04:11Z'}},
                            'lastState': {
                                'terminated': {
                                    'exitCode': 255,
                                    'reason': 'Unknown',
                                    'startedAt': '2022-11-01T10:04:14Z',
                                    'finishedAt': '2022-11-01T12:03:41Z',
                                    'containerID': 'containerd://b010da89c145a03309f43af342774f7c6299c51e4758a8d5416e066d250453b4'
                                }
                            },
                            'ready': True,
                            'restartCount': 67,
                            'image': 'docker.io/library/nginx:latest',
                            'imageID': 'docker.io/library/nginx@sha256:943c25b4b66b332184d5ba6bb18234273551593016c0e0ae906bab111548239f',
                            'containerID': 'containerd://5d297407aef804c35b62734e9b9d03da1f1145c5b45ac12ec8cc216afafef48b',
                            'started': True
                        }
                    ],
                    'qosClass': 'BestEffort'
                }
            }
        ]
    }

    yield mocker.patch('kubernetes.client.rest.RESTClientObject.request', return_value=mocker.Mock(**{
        'status_code': 200,
        'data': json.dumps(response_json)
    }))


def test_kubernetes_api_enrichment_can_be_instantiated():
    assert KubernetesAPIEnrichment() is not None


def test_message_from_var_log_pods_is_enriched(minimal_config, mock_kube_config, mock_api_response_nginx):
    p = KubernetesAPIEnrichment()
    p.init(minimal_config)

    msg = LogMessage("This is the MESSAGE payload")

    #
    # /var/log/pods sets (namespace_name, pod_name, pod_uuid, container_name)
    #
    # sample filename:
    #     /var/log/pods/default_nginx-85b98978db-2dpdh_d7a782cf-cfaa-45e6-8f22-950b2df5bf82/nginx/66.log
    msg['myk8s.namespace_name'] = 'default'
    msg['myk8s.pod_name'] = 'nginx-85b98978db-2dpdh'
    msg['myk8s.pod_uuid'] = 'd7a782cf-cfaa-45e6-8f22-950b2df5bf82'
    msg['myk8s.container_name'] = 'nginx'
    p.parse(msg)

    # fields extracted from the filename stays intact
    assert msg['myk8s.namespace_name'] == b'default'
    assert msg['myk8s.pod_name'] == b'nginx-85b98978db-2dpdh'
    assert msg['myk8s.pod_uuid'] == b'd7a782cf-cfaa-45e6-8f22-950b2df5bf82'
    assert msg['myk8s.container_name'] == b'nginx'

    # fields extracted from the metadata
    assert msg['myk8s.labels.app'] == b'nginx'
    assert msg['myk8s.annotations.cni.projectcalico.org/podIP'] == b'10.1.71.91/32'
    assert msg['myk8s.container_image'] == b'docker.io/library/nginx:latest'


def test_message_from_var_log_containers_is_enriched(minimal_config, mock_kube_config, mock_api_response_nginx):
    p = KubernetesAPIEnrichment()
    p.init(minimal_config)

    msg = LogMessage("This is the MESSAGE payload")

    #
    # /var/log/containers sets (pod_name, namespace_name, container_name, container_id)
    #
    # sample filename:
    # /var/log/containers/nginx-85b98978db-2dpdh_default_nginx-b010da89c145a03309f43af342774f7c6299c51e4758a8d5416e066d250453b4.log
    msg['myk8s.pod_name'] = 'nginx-85b98978db-2dpdh'
    msg['myk8s.namespace_name'] = 'default'
    msg['myk8s.container_name'] = 'nginx'
    msg['myk8s.container_id'] = 'b010da89c145a03309f43af342774f7c6299c51e4758a8d5416e066d250453b4'
    p.parse(msg)

    # fields extracted from the filename stays intact
    assert msg['myk8s.pod_name'] == b'nginx-85b98978db-2dpdh'
    assert msg['myk8s.namespace_name'] == b'default'
    assert msg['myk8s.container_name'] == b'nginx'
    assert msg['myk8s.container_id'] == b'b010da89c145a03309f43af342774f7c6299c51e4758a8d5416e066d250453b4'

    # fields extracted from the metadata
    assert msg['myk8s.labels.app'] == b'nginx'
    assert msg['myk8s.annotations.cni.projectcalico.org/podIP'] == b'10.1.71.91/32'
    assert msg['myk8s.container_image'] == b'docker.io/library/nginx:latest'
    assert msg['myk8s.pod_uuid'] == b'd7a782cf-cfaa-45e6-8f22-950b2df5bf82'


def test_key_delimiter_would_replace_dot(minimal_config, mock_kube_config, mock_api_response_nginx):
    p = KubernetesAPIEnrichment()
    minimal_config['key_delimiter'] = '~'
    minimal_config['prefix'] = 'k8s.'
    p.init(minimal_config)

    msg = LogMessage("This is the MESSAGE payload")

    msg['k8s.pod_name'] = 'nginx-85b98978db-2dpdh'
    msg['k8s.namespace_name'] = 'default'
    p.parse(msg)

    # fields extracted from the metadata
    assert msg['k8s.labels~app'] == b'nginx'
    assert msg['k8s.annotations~cni.projectcalico.org/podIP'] == b'10.1.71.91/32'
    assert msg['k8s.container_image'] == b'docker.io/library/nginx:latest'
    assert msg['k8s.pod_uuid'] == b'd7a782cf-cfaa-45e6-8f22-950b2df5bf82'
