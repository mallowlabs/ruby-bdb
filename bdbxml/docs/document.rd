=begin
== BDB::XML::Document

A Document is the unit of storage within a Container. 

A Document contains a stream of bytes that may be of type XML
or BYTES. The Container only indexes the content of Documents
that contain XML. The BYTES type is supported to allow the storage
of arbitrary content along with XML content.

Document supports annotation attributes.

# module BDB
# module XML
# class Document
# class << self

=== Class Methods

--- allocate
    Allocate a new object

# end

=== Methods

--- self[attr]
    Return the value of an attribute

--- self[attr] = val
    Set the value of an attribute

--- content
    Return the content of the document

--- content=(val)
    Set the content of the document

--- encoding
    Return the encoding

--- get(uri = "", attr)
    Return the value of an attribute. ((|uri|)) specify the namespace
    where reside the attribute.

--- id
    Return the ID

--- initialize(type = BDB::XML::XML)
--- initialize(content = "")
    Initialize the document with the type (or the content) specified

--- name
    Return the name of the document

--- name=(val)
    Set the name of the document

--- prefix
    Return the default prefix for the namespace

--- prefix=(val)
    Set the default prefix used by ((|set|))

--- set(uri = "", prefix = "", attr, value)
    Set an attribute in the namespace ((|uri|)). ((|prefix|)) is the prefix
    for the namespace

--- to_s
--- to_str
    Return the document as a String object

--- uri
    Return the default namespace

--- uri=(val)
    Set the default namespace used by ((|set|)) and ((|get|))

# end
# end
# end

=end