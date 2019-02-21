package io.searchbox.core;

public final class JestIndex extends Index {

  public JestIndex() { super(new Index.Builder(null)); }


  public JestIndex setIndexName(final String index) {
    super.indexName = index;
    return this;
  }

  public JestIndex setId(final String id) {
    super.id = id;
    return this;
  }

  public JestIndex setType(final String type) {
    super.typeName = type;
    return this;
  }

  public JestIndex setFormattedMessage(final String message) {
    super.payload = message;
    return this;
  }

}
