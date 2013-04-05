#include "state_upgrade.c"
#include "testutils.h"

#define XML_STATE "<?xml version=\"1.0\" encoding=\"UTF-8\"?><TestRoot><FileSources><TestFileSource0 CurrentFileName=\"C:\\\\tEst.txt\" CurrentPositionL=\"1024\" CurrentPositionH=\"16\"/><TestFileSource1 CurrentFileName=\"C:\\\\vAr\\\\\\ÁrvÍztŰrŐtükörfúrÓgép.txt\" CurrentPositionL=\"1\" CurrentPositionH=\"1\" /></FileSources></TestRoot>"

void
test_file_name_conversion()
{
  FileState *fs = NULL;
  ConfigStore *store = config_store_new(STORE_TYPE_XML);
  config_store_parse_memory(store, XML_STATE, strlen(XML_STATE), NULL);
  fs = upgrade_tool_get_file_state(store, "FileSources\\TestFileSource0");
  assert_string(fs->filename, "c:\\test.txt", "Bad filename");
  assert_guint64(fs->pos, 0x1000000400, "Bad filepos");
  file_state_free(fs);

  fs = upgrade_tool_get_file_state(store, "FileSources\\TestFileSource1");
  assert_string(fs->filename, "c:\\var\\árvíztűrőtükörfúrógép.txt", "Bad filename");
  assert_guint64(fs->pos, 0x100000001, "Bad filepos");
  file_state_free(fs);

  config_store_free(store);
}

int
main()
{
  test_file_name_conversion();
  return 0;
}
