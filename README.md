# pgexportdoc
command line utility for exporting XML, JSON, BYTEA document from PostgreSQL

This PostgreSQL command line utility (extension) is used for exporting XML, any text or
binary documents from PostgreSQL.

```
pgexportdoc -c 'select x from xmldata where id = 1' -t XML -f myxmldoc.xml
```

for help run
```
pgexportdoc --help
```

XML documents are saved in binary format - decl section with used encoding is attached.

Text documents are write in text format - there are translation from server encoding to
client encoding.

When format is BYTEA, then passing data are in bytea unescaped to binary data.

Attention: The exported documents are completly loaded to client's memory. So you need enough free
memory on client, when you would to use this tool. Maximal teoretical size of exported document
is 1GB. More practical real maximal size is about 100MB.

ToDo:

* regress tests

Examples:

```
```

Pavel Stehule, 2017 pavel.stehule@gmail.com Czech Republic, Prague
