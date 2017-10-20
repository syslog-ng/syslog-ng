from source.testdb.initializer.setup_classes import SetupClasses


def init_unittest(request, topology):
    sc = SetupClasses(request, topology=topology)
    return sc


def test_server_topology(request):
    sc = init_unittest(request, topology="server")
    assert hasattr(sc, 'testdb_path_database_for_server') is True
    assert hasattr(sc, 'testdb_path_database_for_client') is False


def test_client_server_topology(request):
    sc = init_unittest(request, topology="client_server")
    assert hasattr(sc, 'testdb_path_database_for_server') is True
    assert hasattr(sc, 'testdb_path_database_for_client') is True
