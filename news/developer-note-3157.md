LightReports: Support to relocate reports dir other than current base dir
This will work:
`python -m pytest -lvs functional_tests/source_drivers/file_source/test_acceptance.py --installdir=/install --reports /tmp/`
