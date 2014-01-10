<?php
// this is a handler. it gets extended by code that handles a db syntax, such as mysql or mssql.
// the handler has one or more connections.
// once constructed, it is tied/connected to a particular database.


// todo free results, free statements


abstract class sql { // must be extended by sql_mssql or sql_mysqli for example.

	protected static $connections = array();
	protected $data_source;
	public $data_source_name;
	const STRING_DELIMITER = "'"; // single quotes should be pretty universal.

	function __construct(data_source $data_source) {
		if(m::help('sql', 4)) m::dump('constructing a ' . $data_source->type . ' handler for ' . $data_source->name);
		$this->data_source = $data_source;
		$this->data_source_name = $this->data_source->name;
	}

	final function get_connection() {
		if( ! isset($this->data_source)) throw new Exception('missing data source info');
		if(isset(self::$connections[$this->data_source->name])) return self::$connections[$this->data_source->name];
		// okay, make a new connection.
		if(m::help('sql', 5)) m::dump($this->data_source, 'making ' . get_class($this) . ' connection');
		//m::dump($this->data_source, 'ds');
		$this->_get_connection();
		return self::$connections[$this->data_source->name];
	}

	function write_query(query $query) {
		// logging? universal checking?
		// if a skip is specified, we are going to require an order by clause. this is primarily because tsql will require it, but also because it is good sense.
		if(is_numeric($query->skip) and $query->skip > 0) {
			if(empty($query->clauses['order_by'])) throw new Exception('must have order by specified if you are skipping records.');
		}
		// normally this is called with en empty query. if it is called outside that logic, we'll protect what we find. this is mostly for debugging
		if(strlen($query->query)) $query->original_query = $query->query;
		$query->query = '';
		// (this should always be called by child class)
	}

	function write_update_query(query $query) {
		if(empty($query->data_table_name)) throw new Exception('must provide data_table_name as in UPDATE data_table_name');
		if(empty($query->clauses['where'])) throw new Exception('update queries must include where clauses.');

		$query->query = "update {$query->data_table_name} set ";

		// we are going to flesh out an array, such that the keys are field names, and the values are values, including delimiters.
		if(count($query->data)) {
			$pairs = array();
			$data_types = array_keys($query->data);
			foreach($data_types as $data_type) {
				switch($data_type) {
					case 'null':
						foreach($query->data[$data_type] as $field_name => $value) { // value should always be new. todo throw error on not null?
							$pairs[$field_name] = 'NULL';
						}
						break;
					case 'code': // like string, but allows angle brackets, quotes, etc.
						foreach($query->data[$data_type] as $field_name => $value) {
							$pairs[$field_name] = "'" . static::groom_code_data($value) . "'";
						}
						break;
					case 'string':
						foreach($query->data[$data_type] as $field_name => $value) {
							$pairs[$field_name] = "'" . static::groom_string_data($value) . "'";
						}
						break;
					case 'integer':
					case 'float':
						foreach($query->data[$data_type] as $field_name => $value) {
							$orig_value = $value;
							$numeric_value = (float) $value;
							if ($numeric_value != $orig_value) throw new Exception("non numeric data passed as $data_type.");
							$pairs[$field_name] = $numeric_value;
						}
						break;
					case 'unmodified':
					case 'custom':
					case 'raw':
					case 'reference':  //reference to another field.
						foreach($query->data[$data_type] as $field_name => $value) $pairs[$field_name] = $value;
						break;

					default:
						throw new Exception('unsupported data type for update query construction: ' . $data_type);
						break;
				}
			}

			// now convert key value pairs to a list of key = value
			// remember, $value should already have quotes wrapped aorund it if nec.
			$list = array();
			foreach($pairs as $key => $value) $list[] = $key . ' = ' . $value;

			$query->query .= ' ' . implode(', ', $list) . ' WHERE ' . $query->get_final_where_clause();

		}
	}

	function write_insert_query(query $query) {
		// note - mysql seems to require empty sets if there is no data: insert into person () values ()
		if(empty($query->data_table_name)) throw new Exception('must provide data_table_name as in INSERT INTO data_table_name');
		if( ! in_array($query->action, array('insert', 'replace'))) throw new Exception('trying to write insert query for a ' . $query->action . ' query');
		$query->query .= strtoupper($query->action) . ' into ' . $query->data_table_name . ' ';
		// we are going to flesh out these two arrays, and they become the INSERT INTO (fieldnames) VALUES (values) [, (values)[, (values)]]
		$field_names = array(); // this will hold the first part of that.
		$values = array(); // will hold the last part of that.
		if(count($query->data)) {
			// is it one row? or many. if one, we have an assoc arry. if many, we have an array of them.
			// for now i'll just see if the first array key is a word
			$temp = array_keys($query->data);
			if( ! is_numeric($temp[0])) $query->data = array($query->data);
			foreach($query->data as $row) {
				if( ! isset($data_types)) $data_types = array_keys($row);
				// the data types and field names must be the same for every record.... just in case someone set up the data wrong...
				if($data_types !== array_keys($row)) throw new Exception('bad insert data structure. datatypes must be same for every row.');
				$field_names_loop = array(); // reset every time
				$values_loop = array(); // reset every time
				foreach($data_types as $data_type) {
					switch($data_type) {
						case 'null':
							foreach($row[$data_type] as $field_name => $value) { // value should always be NULL. todo throw error on not null?
								$field_names_loop[] = $field_name;
								$values_loop[] = 'NULL';
							}
							break;
						case 'code':
							foreach($row[$data_type] as $field_name => $value) {
								$field_names_loop[] = $field_name;
								$values_loop[] = "'" . static::groom_code_data($value) . "'";
							}
							break;
						case 'date':
							foreach($row[$data_type] as $field_name => $value) {
								$field_names_loop[] = $field_name;
								// if you gave me a date object, i will format it properly, otherwise i assume you did something acceptable.
								if(is_object($value) and $value instanceof DateTime) {
									$values_loop[] = static::groom_date_data($value);
								} else {
									$values_loop[] = "'" . static::groom_string_data($value) . "'";
								}
							}
							break;
						case 'string':
							$connection = self::get_connection();
							foreach($row[$data_type] as $field_name => $value) {
								$field_names_loop[] = $field_name;
								$values_loop[] = "'" . static::groom_string_data($value) . "'";
							}
							break;
						case 'integer':
						case 'float':
							foreach($row[$data_type] as $field_name => $value) {
								$orig_value = $value;
								$numeric_value = (float) $value;
								if((string) $numeric_value != $orig_value) throw new Exception("non numeric data passed as $data_type.");
								$field_names_loop[] = $field_name;
								$values_loop[] = $numeric_value;
							}
							break;
						case 'custom':
						case 'unmodified':
						case 'raw':
						case 'reference':  //reference to another field.
							foreach($row[$data_type] as $field_name => $value) {
								$field_names_loop[] = $field_name;
								$values_loop[] = $value;
							}
							break;

						default:
							throw new Exception('unsupported data type for insert query construction: ' . $data_type);
							break;
					}
				}
				if( ! isset($final_field_names)) {
					$field_names = $field_names_loop;
				} else if($field_names !== $field_names_loop) throw new Exception('bad insert data structure. keys must be same name every row.');
				$values[] = $values_loop;
			}

		}
		// m::dump($field_names); m::dump($values);

		// yes, these go outside, as mysql seems to prefer something like insert into xyz () values () for inserting empty records.
		$query->query .= '(' . implode(', ', $field_names) . ')';
		$query->query .= ' values ';
		if(empty($values)) {
			$query->query .= ' ()';
		} else {
			$temp = array();
			foreach($values as $value_row) $temp[] = '(' . implode(', ', $value_row) . ')';
			$query->query .= implode(', ', $temp);
		}
	}

	function prepare($query) { throw new Excpetion('prepared statements not implemented.'); }

	function run(query $query) {
		if(m::help('sql', 9)) m::dump('running a ' . get_class($this) . ' query: ' . $query->query);
		// logging should be done here.
		if(empty($query->query)) {
			$this->write_query($query);
		}
	}

	abstract protected function _get_connection();

	protected function _trigger_error(query $query) {
		$query->error_messages[] = get_class($this);
		$query->error_messages[] = $query->query;
		$query->error_messages[] = implode(' X ' , $query->error_messages);
		$query->error_messages[] = 'SEND_EMAIL'; // todo temporary placewise thing
		$temp = implode(' | ', $query->error_messages);
		if(m::help('sql')) m::dump($query->error_messages, 'ERRORS on this ' . get_class($this) . ' query: ' . $query->query);
		if( ! $query->silent) trigger_error($temp, E_USER_WARNING); // upgrade to ERROR and kill sendmail when possible.
	}

	public function groom_date_data(DateTime $date) { // todo - if this needs to be publicly avail, need to check for data source, etc
		throw new Exception('code not yet developed for dates. you can format the date yourself if you want.');
	}

	/**
	* you must extend this so that we have access to things like mysql_real_escape_stirng(). but we'll handle some basics here, such as double and single quotes
	*
	* @param string $str
	*/
	public function groom_string_data($str) {
		$str = html_entity_decode($str, ENT_NOQUOTES, 'UTF-8'); // when in php 5.4, add | ENT_HTML5
		$str = str_replace(array("'", '"'), array('&#039;', '&quot;'), $str);
		return $str;
	}

	public function groom_code_data($str) { // just like string data, except that we leave quotes alone.
		$str = html_entity_decode($str, ENT_NOQUOTES, 'UTF-8'); // when in php 5.4, add | ENT_HTML5
		return $str;
	}

	public function groom_result(query $query) {
		if( ! $query->executed or ! $query->success) return false;
		// okay, if it was a select, that's easy. if it was an insert, grab the new id. if it was an update.... or a delete... return rows affected?
		// we could judge by the ->action, but that could be misleading, and isn't always conclusive
		// to judge the true action, we'll look at the first six letters. which means we should strip any comments and whitespace
		$query->executed_action = $query->action; // hold on.....
		if(! in_array($query->executed_action, array('select', 'update', 'insert', 'replace', 'delete', 'show', 'create'))) $query->executed_action = substr(trim(preg_replace("/\/\*.*?\*\//", '', $query->query)), 0, 6); // feel free to extend the array.... /the regex removes any /* comments */ that might interfere

		// some things are universal:
		switch($query->executed_action) {
			case 'insert':
			case 'replace': // todo replace not tested outside of mysqli
				// if the table has an auto increment id field, we want to return it. if not, we return boolean about the success of the insert.
				// so we will ask for the id, which was appended to the query. if it is null, it just means there wasn't one, return true.
				// mysqli already has this value, sql server, you gotta ask for it.... i could extend teh ask for it, but i think i'll just check if it is already set.
				if(isset($query->insert_id)) { // insert_id should be a number, or true for tables that have no pk/autoinc or false for errors
					$query->result = $query->insert_id;
				} else {
					// okay, we'll ask for it.
					$query->insert_id = $this->get_last_insert_id(); // returns null or number
					// if there is no id, that doesn't main failure, some tables don't have it. but we don't want to return false - that implies error. so we do true.
					if(is_numeric($query->insert_id)) {
						$query->result = $query->insert_id;
					} else {
						$query->result = true;
					}
				}
				break;
		}

		// now the child will take over, as long as we didn't set a result.

	}

	function replace_tokens($string = '', $values = array()) {
		if(empty($string)) return '';
		// we check for empty values only if the string has tokens.
		if($how_many_vars = substr_count($string, '%')) {
			if(empty($values)) trigger_error('no values supplied for token replacement.', E_USER_ERROR);
			// we should have as many %s as we do $values.
			// won't work if some text has casual percents in it. if( ! is_array($values) or count($values) != $how_many_vars) throw new Exception('var to marker mismatch');
			$count = 0;
			while($token = $this->get_token($string) and count($values)) {
				$value = array_shift($values); // this used to be in the conditional above, but that disallows empty values for replacing tokens with.
				// as you go thru this switch, set groomed_data, or throw an exception.
				switch($token) {
					case '%s':
						// replace the token with an escaped and quoted string.
						$groomed_data = static::STRING_DELIMITER . $this->groom_string_data($value) . static::STRING_DELIMITER;
						break;
					case '%d':
						// replace the token with an integer
						$original_data = $value;
						$groomed_data = floatval($original_data);
						if(is_string($original_data) and strval($groomed_data) !== $original_data) throw new Exception('bad data for integer. ');
						break;
					default:
						throw new Exception('un developed token (' . $token . ') for sql');
						break;
				}
				$string = preg_replace("/$token/", $groomed_data, $string, 1); // this is how we limit it to one.
			}
			return $string;

		} else {
			// fragment unchanged.
			return $string;
		}
	}

	function get_token($str) {
		if(preg_match('/(%(s|d))/', $str, $matches)) return $matches[0];
		return false;
	}


	abstract function check_for_schema($schema_name);
}

/**
* i was toying with this, i don't think it should survive.
*/
function get_empty_data_for_sql() {
	return array('integer' => array(), 'string' => array(), 'null' => array());
}

/**
* quick check to see if there is anything nested down in there.
* @param array $data
*/
function sql_data_has_data($data) {
	foreach(array_keys($data) as $data_type) if(count($data[$data_type])) return true;
		return false;
}

/**
* @returns boolean indicating an associative array was passed.
*	pretty quick and dirty, only checks first key, which may not even exist.
* @param array $array
*/
function sql_is_assoc($array) {
	$temp = array_keys($array);
	return is_numeric($temp[0]);
}