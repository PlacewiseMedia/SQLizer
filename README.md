SQLizer
======

Classes for accessing multiple sql syntaxes ... coupled to m ... and it is all in php.

Many parts of this code base are only developed as far as we need them to be. It is incomplete. Use at your own risk.

When working with queries, we use lists of things a lot. You select a list of field names. You update a list of field names and provide a list of lists of values. All of these list-like things are set as arrays.

We also need to group somethings together, those are typically done with associative arrays.

Any list that involves key-value pairs is always an associative array.
The weirdest part of the query-as-object is the where clauses.

### WHERE clause ###
In its least abstracted form, simply pass in a string: 'where uid = 7'
In its most abstracted form, we use an associative array of clauses and arrays of values to sprintf the clause. Each element of the array is concatenated together by an AND.

Each key is something like
* ```'uid = 7' // (empty value)```
* ```'uid = %d' => array($uid) // just like sprintf, we pass in an array of the values in the order they are used.```
* ```"name like '%%%s%%'" => array($name) // just like sprintf, we pass in an array of the values in the order they are used. double percents to escape them.```
why? so that we can do this:
* ```"uid in (%d, %d, %d) or name in('%s', '%s')" => array($uid1, $uid2, $uid3, $name, $name_alt)```
Note that this is how we do ORs in our wheres, at this time.

### data structure for INSERTs and UPDATEs ###
updates and inserts use a specific data structure that groups the fieldname/value pairs by data type.
I recommend you build it before you instantiate the query. Here is a giant sample with all known data types.
```php
$data = array(
    'integer' => array('fieldname' => 123), // uses cast to string and back to float to determine that value is numeric. actually sends a float to the db.
    'float' => array('fieldname' => 123.876), // identical to above.
    'date' => array('fieldname' => $expire_date), // don't bother with this type unless your value is a php DateTime object. if not, use string and format it yourself (lame).
    'string' => array('fieldname' => $text), // html entities are converted to utf-8. single and double quotes are converted to html entities.
    'code' => array('fieldname' => $html), // html entities are converted to utf-8. single and double quotes are escaped so that they go into the db unchanged. use this for html or code snippets ONLY
    'null' => array('fieldname' => (any value here is ignored) // when you want the field to be set to null.
);
```
Some data type names are aliases, but the above are the canonical.
integer and float do the same thing behind the scenes.
unmodified, raw, & reference (all aliases) exist so that you can do complex things like reference other columns (eg update tablename set field1 = field2) please do not use them for other reasons.

### Use the shortcuts! ###
```$query = new select_query('db_name', '*', 'users', array('uid = %d' => array($uid));```

```$query = new insert_query('db_name', 'users', $data);```

One of my favorite shortcuts is the $return_format, which defaults to 'array of associative arrays.' Use it to get your data faster. If you know you will return one row, and one field which is a numeric id, set your query up to use a return_format of 'single integer'

```php
$query = new select_query('db_name', 'uid', 'users', array('mail = %s' => array($email), array('return_format' => 'single integer')));

$uid = db::q($query); // $uid will be false on error, null on not found, or an int (I think) if found.
```
A warning will be issued if more than one row is returned. to suppress the warning, use 'single integer regardless'

The return format changes the query->result, which is what is returned by db::q();

The return formats are a little weird in that they don't replace spaces with underscores. [shrug]
* array of integers - select a single field, it will be typecast to int
* array of strings - select a single field
* array of associative arrays - default
* single associative array - like default but saves you from using $result[0]
* single string
* single string regardless - suppresses warnings on multiple records
* single integer
* single integer regardless - suppresses warnings on multiple records
* pk value array - use aliases in your query such that one field is named pk and the other value. they will be rearranged as key-value pairs in an (associative?) array.

### Caveat Programor ###
All features of this repo are most fleshed out for mysql, followed by mssql, sqlsrv, and finally, pgsql, which is not yet usable.

Written primarily by Alex Brown.