driver_database = {
    "amqp": {
        "used_as_source_driver_in_following_product_versions": [],
        "used_as_destination_driver_in_following_product_versions": ['ose_3.5', 'ose_3.6', 'ose_3.7'],
        "used_source_resource": {},
        "used_destination_resource": {"ip_address": "127.0.0.1", "port": "5672", "vhost": "/", "exchange": "syslog",
                                      "exchange_type": "fanout"},
        "required_source_options": [],
        "required_destination_options": [
            {
                "option_name": "username",
                "option_value": ""
            },
            {
                "option_name": "password",
                "option_value": ""
            },
        ],
        "working_type": [],
    },
    "file": {
        "used_as_source_driver_in_following_product_versions": ['3.8'],
        "used_as_destination_driver_in_following_product_versions": ['3.8'],
        "used_source_resource": {"local": ""},
        "used_destination_resource": {"local": ""},
        "required_source_options": [
            {
                "option_name": "file_path",
            },
        ],
        "required_destination_options": [
            {
                "option_name": "file_path",
            },
        ],
        "working_type": ['file'],
    },
    "internal": {
        "used_as_source_driver_in_following_product_versions": ['ose_3.5', 'ose_3.6', 'ose_3.7'],
        "used_as_destination_driver_in_following_product_versions": [],
        "used_source_resource": [],
        "used_destination_resource": [],
        "required_source_options": [],
        "required_destination_options": [],
        "working_type": [],
    },
    "mongodb": {
        "used_as_source_driver_in_following_product_versions": [],
        "used_as_destination_driver_in_following_product_versions": ['ose_3.5', 'ose_3.6', 'ose_3.7'],
        "used_source_resource": {},
        "used_destination_resource": {"ip_address": "127.0.0.1", "port": "27017", "database": "syslog",
                                      "collection": "messages"},
        "required_source_options": [],
        "required_destination_options": [],
        "working_type": [],
    },
    "network": {
        "used_as_source_driver_in_following_product_versions": ['ose_3.5', 'ose_3.6', 'ose_3.7'],
        "used_as_destination_driver_in_following_product_versions": ['ose_3.5', 'ose_3.6', 'ose_3.7'],
        "used_source_resource": {"ip_address": "127.0.0.1", "port": "11111"},
        "used_destination_resource": {"ip_address": "127.0.0.1", "port": "22221"},
        "required_source_options": [
            {
                "option_name": "port",
                "option_value": "11111",
            },
        ],
        "required_destination_options": [
            {
                "option_name": "port",
                "option_value": "22221",
            },
        ],
        "working_type": [],
    },
    "pipe": {
        "used_as_source_driver_in_following_product_versions": ['ose_3.5', 'ose_3.6', 'ose_3.7'],
        "used_as_destination_driver_in_following_product_versions": ['ose_3.5', 'ose_3.6', 'ose_3.7'],
        "used_source_resource": {"local": ""},
        "used_destination_resource": {"local": ""},
        "required_source_options": [
            {
                "option_name": "file_path",
            },
        ],
        "required_destination_options": [
            {
                "option_name": "file_path",
            },
        ],
        "working_type": ['file'],
    },
    "program": {
        "used_as_source_driver_in_following_product_versions": ['ose_3.5', 'ose_3.6', 'ose_3.7'],
        "used_as_destination_driver_in_following_product_versions": ['ose_3.5', 'ose_3.6', 'ose_3.7'],
        "used_source_resource": {"local": ""},
        "used_destination_resource": {"local": ""},
        "required_source_options": [
            {
                "option_name": "file_path",
            },
        ],
        "required_destination_options": [
            {
                "option_name": "file_path",
            },
        ],
        "working_type": ['file'],
    },
    "pseudofile": {
        "used_as_source_driver_in_following_product_versions": [],
        "used_as_destination_driver_in_following_product_versions": ['ose_3.6', 'ose_3.7'],
        "used_source_resource": [],
        "used_destination_resource": [],
        "required_source_options": [],
        "required_destination_options": [],
        "working_type": [],
    },
    "redis": {
        "used_as_source_driver_in_following_product_versions": [],
        "used_as_destination_driver_in_following_product_versions": ['ose_3.5', 'ose_3.6', 'ose_3.7'],
        "used_source_resource": {},
        "used_destination_resource": {"ip_address": "127.0.0.1", "port": "6379"},
        "required_source_options": [],
        "required_destination_options": [],
        "working_type": [],
    },
    "riemann": {
        "used_as_source_driver_in_following_product_versions": [],
        "used_as_destination_driver_in_following_product_versions": ['ose_3.6', 'ose_3.7'],
        "used_source_resource": {},
        "used_destination_resource": {"ip_address": "127.0.0.1", "port": "5555"},
        "required_source_options": [],
        "required_destination_options": [],
        "working_type": [],
    },
    "smtp": {
        "used_as_source_driver_in_following_product_versions": [],
        "used_as_destination_driver_in_following_product_versions": ['ose_3.5', 'ose_3.6', 'ose_3.7'],
        "used_source_resource": {},
        "used_destination_resource": {"ip_address": "127.0.0.1", "port": "25"},
        "required_source_options": [],
        "required_destination_options": [
            {
                "option_name": "from",
                "option_value": "from@from.com"
            },
            {
                "option_name": "to",
                "option_value": "to@to.com"
            },
            {
                "option_name": "subject",
                "option_value": "subject"
            },
            {
                "option_name": "body",
                "option_value": "body"
            },
        ],
        "working_type": [],
    },
    "sql": {
        "used_as_source_driver_in_following_product_versions": [],
        "used_as_destination_driver_in_following_product_versions": ['ose_3.5', 'ose_3.6', 'ose_3.7'],
        "used_source_resource": [],
        "used_destination_resource": [],
        "required_source_options": [],
        "required_destination_options": [],
        "working_type": [],
    },
    "stomp": {
        "used_as_source_driver_in_following_product_versions": [],
        "used_as_destination_driver_in_following_product_versions": ['ose_3.5', 'ose_3.6', 'ose_3.7'],
        "used_source_resource": [],
        "used_destination_resource": {"ip_address": "127.0.0.1", "port": "61613", "destination": "/topic/syslog"},
        "required_source_options": [],
        "required_destination_options": [],
        "working_type": [],
    },
    "sun_streams": {
        "used_as_source_driver_in_following_product_versions": ['ose_3.5', 'ose_3.6', 'ose_3.7'],
        "used_as_destination_driver_in_following_product_versions": [],
        "used_source_resource": [],
        "used_destination_resource": [],
        "required_source_options": [
            {
                "option_name": "file_path",
            },
        ],
        "required_destination_options": [],
        "working_type": [],
    },
    "syslog": {
        "used_as_source_driver_in_following_product_versions": ['ose_3.5', 'ose_3.6', 'ose_3.7'],
        "used_as_destination_driver_in_following_product_versions": ['ose_3.5', 'ose_3.6', 'ose_3.7'],
        "used_source_resource": {"ip_address": "127.0.0.1", "port": "11112"},
        "used_destination_resource": {"ip_address": "127.0.0.1", "port": "22222"},
        "required_source_options": [
            {
                "option_name": "port",
                "option_value": "11112",
            },
        ],
        "required_destination_options": [
            {
                "option_name": "port",
                "option_value": "22222",
            },
        ],
        "working_type": [],
    },
    "systemd_journal": {
        "used_as_source_driver_in_following_product_versions": ['ose_3.6', 'ose_3.7'],
        "used_as_destination_driver_in_following_product_versions": [],
        "used_source_resource": [],
        "used_destination_resource": [],
        "required_source_options": [],
        "required_destination_options": [],
        "working_type": [],
    },
    "systemd_syslog": {
        "used_as_source_driver_in_following_product_versions": ['ose_3.6', 'ose_3.7'],
        "used_as_destination_driver_in_following_product_versions": [],
        "used_source_resource": [],
        "used_destination_resource": [],
        "required_source_options": [],
        "required_destination_options": [],
        "working_type": [],
    },
    "tcp": {
        "used_as_source_driver_in_following_product_versions": ['ose_3.5', 'ose_3.6', 'ose_3.7'],
        "used_as_destination_driver_in_following_product_versions": ['ose_3.5', 'ose_3.6', 'ose_3.7'],
        "used_source_resource": {"ip_address": "127.0.0.1", "port": "11113"},
        "used_destination_resource": {"ip_address": "127.0.0.1", "port": "22223"},
        "required_source_options": [
            {
                "option_name": "port",
                "option_value": "11113",
            },
        ],
        "required_destination_options": [
            {
                "option_name": "port",
                "option_value": "22223",
            },
        ],
        "working_type": [],
    },
    "tcp6": {
        "used_as_source_driver_in_following_product_versions": ['ose_3.5', 'ose_3.6', 'ose_3.7'],
        "used_as_destination_driver_in_following_product_versions": ['ose_3.5', 'ose_3.6', 'ose_3.7'],
        "used_source_resource": {"ip_address": "::1", "port": "11114"},
        "used_destination_resource": {"ip_address": "::1", "port": "22224"},
        "required_source_options": [
            {
                "option_name": "port",
                "option_value": "11114",
            },
        ],
        "required_destination_options": [
            {
                "option_name": "port",
                "option_value": "22224",
            },
        ],
        "working_type": [],
    },
    "udp": {
        "used_as_source_driver_in_following_product_versions": ['ose_3.5', 'ose_3.6', 'ose_3.7'],
        "used_as_destination_driver_in_following_product_versions": ['ose_3.5', 'ose_3.6', 'ose_3.7'],
        "used_source_resource": {"ip_address": "127.0.0.1", "port": "11115"},
        "used_destination_resource": {"ip_address": "127.0.0.1", "port": "22225"},
        "required_source_options": [
            {
                "option_name": "port",
                "option_value": "11115",
            },
        ],
        "required_destination_options": [
            {
                "option_name": "port",
                "option_value": "22225",
            },
        ],
        "working_type": [],
    },
    "udp6": {
        "used_as_source_driver_in_following_product_versions": ['ose_3.5', 'ose_3.6', 'ose_3.7'],
        "used_as_destination_driver_in_following_product_versions": ['ose_3.5', 'ose_3.6', 'ose_3.7'],
        "used_source_resource": {"ip_address": "::1", "port": "11116"},
        "used_destination_resource": {"ip_address": "::1", "port": "22226"},
        "required_source_options": [
            {
                "option_name": "port",
                "option_value": "11116",
            },
        ],
        "required_destination_options": [
            {
                "option_name": "port",
                "option_value": "22226",
            },
        ],
        "working_type": [],
    },
    "unix_dgram": {
        "used_as_source_driver_in_following_product_versions": ['ose_3.5', 'ose_3.6', 'ose_3.7'],
        "used_as_destination_driver_in_following_product_versions": ['ose_3.5', 'ose_3.6', 'ose_3.7'],
        "used_source_resource": {"local": ""},
        "used_destination_resource": {"local": ""},
        "required_source_options": [
            {
                "option_name": "file_path",
            },
        ],
        "required_destination_options": [
            {
                "option_name": "file_path",
            },
        ],
        "working_type": [],
    },
    "unix_stream": {
        "used_as_source_driver_in_following_product_versions": ['ose_3.5', 'ose_3.6', 'ose_3.7'],
        "used_as_destination_driver_in_following_product_versions": ['ose_3.5', 'ose_3.6', 'ose_3.7'],
        "used_source_resource": {"local": ""},
        "used_destination_resource": {"local": ""},
        "required_source_options": [
            {
                "option_name": "file_path",
            },
        ],
        "required_destination_options": [
            {
                "option_name": "file_path",
            },
        ],
        "working_type": [],
    },
    "usertty": {
        "used_as_source_driver_in_following_product_versions": [],
        "used_as_destination_driver_in_following_product_versions": ['ose_3.5', 'ose_3.6', 'ose_3.7'],
        "used_source_resource": [],
        "used_destination_resource": [],
        "required_source_options": [],
        "required_destination_options": [],
        "working_type": [],
    },
}
