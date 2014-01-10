<?php

class sql_mysqli extends sql {

	protected function _get_connection() { // called by get_connection. don't break that flow.
		if($this->data_source->name == 'test_dummy') {
			// for internal debugging and dev, eg write query for debug()
			self::$connections[$this->data_source->name] = new mysqli(); // todo - this will use defaults set in php.ini, which may be okay, but prob not.
			return;
		}
		self::$connections[$this->data_source->name] = new mysqli($this->data_source->host, $this->data_source->username, $this->data_source->get_password(), $this->data_source->get_default_schema(), $this->data_source->port);
		if (mysqli_connect_error()) {
			throw new Exception('mysqli db connection to ' . $this->data_source->name . ' datasource failed: (' . mysqli_connect_errno() . ') ' . mysqli_connect_error());
		}
		self::$connections[$this->data_source->name]->set_charset('utf8');
	}

	function write_query(query $query) {
		parent::write_query($query); // logging and type checking, sets ->query to empty string.

		// only one type of action is allowed. select, insert, update, delete, etc.
		switch($query->action) {
			case 'select':
				// so, put the query clauses together. here is the order for mysql
				foreach(array('select', 'from', 'where', 'having', 'order_by', 'limit', 'note') as $clause_name) {
					if( ! empty($query->clauses[$clause_name])) $query->query .= ' ' . strtoupper(str_replace('_', ' ', $clause_name)) . ' ';
					switch($clause_name) {

						case 'select':
							// a select clause can also carry join information. and in those joins the 'FULL' keyword is not universal, but also pretty uncommon.
							// we'll handle these at the query level.
							// note tsql adds a top, but we don't need that.
							$query->query .= $query->get_final_select_clause();
						break;

						case 'where':
							// a where clause is essentially universal. there may be some obscure comparison operators, but by and large, we can use these in the same way every where. so.... we'll just have the query do this part.
							$query->query .= $query->get_final_where_clause();
						break;

						case 'order_by':
							if( ! empty($query->clauses[$clause_name])) {
								if(is_array($query->clauses[$clause_name]) and count($query->clauses[$clause_name])) {
									$temp = array();
									foreach($query->clauses[$clause_name] as $fields => $direction) $temp[] = $fields . ' ' . $direction;
									$query->query .= implode(', ', $temp);
								} else {
									$query->query .= $query->clauses[$clause_name];
								}
							}
						break;

						case 'from':
							if(empty($query->clauses[$clause_name])) throw new Exception($query->action . ' needs a ' . $clause_name . ' clause.');
							$query->query .= $query->clauses[$clause_name];
						break;

						case 'limit':
							if($query->limit > 0) {
								$query->query .= ' limit ' . $query->skip . ', ' . $query->limit;
							} else {
								$query->note .= ' no limit! should we stop this in the writing? ';
							}
						break;

						case 'note':
							if( ! empty($query->note)) $query->query .= ' /* ' . $query->note . ' */ ';
						break;

						default:
							if( ! empty($query->clauses[$clause_name])) $query->query .= strtoupper($clause_name) . ' ' . $query->clauses[$clause_name];
						break;
					}
					if( ! empty($query->clauses[$clause_name . '_note'])) $query->query .= '/* ' . $query->clauses[$clause_name . '_note'] . ' */ ';
				}
			break;

			case 'insert':
			case 'replace':
				$this->write_insert_query($query);
			break;

			case 'update':
				$this->write_update_query($query);
			break;

			case 'delete':

				if(empty($query->data_table_name)) throw new Exception('must provide data_table_name as in DELETE from data_table_name.');
				if(empty($query->clauses['where'])) throw new Exception('delete queries must include where clauses.');

				$query->query = "delete from {$query->data_table_name} where " . $query->get_final_where_clause();
				// a where clause is essentially universal. there may be some obscure comparison operators, but by and large, we can use these in the same way every where. so.... we'll just have the query do this part.

				//m::death($query, 'how does the query look?');
			break;

			default:
				throw new Exception($query->action . ' not implemented.');
			break;
		}

		return $query->query;
	}

	function run(query $query) {
		parent::run($query); // mostly for logging and debugging - writes query if nec., gets connection
		// todo - check for stored procedure?

		// do we have a connection?
		$connection = self::get_connection();

		/////////// this line runs your query.
		if($result = $connection->query($query->query)) {
			$query->raw_result = $result;
			$query->success = true;
			$query->executed = true;
			// if it was a insert query, we want the insertid. unles it was a table that had no auto inc or primary key, in which case we want bool true, if successful
			if($query->action == 'insert' or $query->action == 'replace') {
				if($query->success) {
					// insert id is a number. zero means no key
					if($connection->insert_id) {
						$query->insert_id = $connection->insert_id;
					} else {
						$query->insert_id = true;
					}
				} else {
					$query->insert_id = false;
				}
			}
			$query->num_rows = is_bool($result) ? NULL : (float) $result->num_rows; // num rows throws notices on inserts and updates.
			return $result;
		} else {
			if(m::help('sql')) m::dump($connection->error_list, 'ERRORS on this query: ' . $query->query); // normally this is done solely in the _trigger_error, but there's all this extra info....
			$query->error_messages[] = $connection->error;
            // are we able to know the nature of the error? i think if we can classify it to a few things,
            // we can better allow our consumers to make decisions
            // our error types: connection, table_existence, unknown field.
            // for mysql, le'ts look for some key terms in the error message.
            if(stripos($connection->error, 'doesn\'t exist') !== false) {
                // no table.
                $query->error_type = 'table_existence'; // const? make it not readable. enum? todo
            }
			$this->_trigger_error($query);
			return false;
		}
	}

	public function groom_date_data(DateTime $date) {
		// return sting in something like standard ANSI format; '20120618 10:34:09 AM'
		return "'" . $date->format('Y-m-d H:i:s') . "'";
	}


	/**
	* pass me a string, and i will groom single and double quotes to html entities, but convert all other html entities to utf8. then i will mysqli_escape it. after this you will still need to wrap it in quotes, but you can use either single or double.
	*
	* @returns database-safe string.
	*
	* @param string $str
	*/
	public function groom_string_data($str) { // todo - if this needs to be publicly avail, need to check for data source, etc
		$str = parent::groom_string_data($str); // handles non-mysql grooming, such as entities and quotes.
		$connection = static::get_connection(); // todo make sure i have a string, and a connection.
		return $connection->real_escape_string($str);
	}

	public function groom_code_data($str) { // just like string data, except that we leave quotes alone. (see parent)
		$str = parent::groom_code_data($str); // handles non-mysql grooming, such as entities.
		$connection = static::get_connection(); // todo make sure i have a string, and a connection.
		return $connection->real_escape_string($str);
	}


	/**
	* this is broken into a buncha switches, please pay attention
	*
	* @param query $query
	*/
	public function groom_result(query $query) {
		parent::groom_result($query); // sets -executed_action and makes sure we executed
		// okay, if it was a select, that's easy. if it was an insert, grab the new id. if it was an update.... or a delete... return rows affected?
		// the parent function just set executed action, go by that
		if( ! isset($query->result)) {
			switch($query->executed_action) {
				case 'select':
				case 'show':
					$data = array();
					while($row = $query->raw_result->fetch_assoc()) $data[] = $row;

					if(count($data) === 0) {
						$query->set_empty_result(); // this does things like NULL vs empty array. it is nicer than the same thing all teh time.
					} else {
						$query->set_results($data); // we have some results
					}

				break;

				// INSERTs handled by parent.

				case 'update':
				case 'delete':
					$connection = $this->get_connection();
					if($query->raw_result === true) {
						$info = $connection->info; // ex Rows matched: 1 Changed: 0 Warnings: 0
						$info = str_replace('Rows matched', 'rows_matched', strtolower($info)); // ex rows_matched: 1 changed: 0 warnings: 0
						$info = str_replace(': ', ':', $info); // ex Rows matched:1 Changed:0 Warnings:0
						$info = explode(' ', $info);
						$query->rows_matched = isset($info['rows_matched']) ? (float) $info['rows_matched'] : NULL;
						$query->changed = isset($info['changed']) ? (float) $info['changed'] : NULL;
						// let's set the final reutrn as the number of matched rows, unless it is zero, inwhich case return true.
						$query->result = $query->rows_matched ? $query->rows_matched : true;
					}
				break;

				default:
					throw new Exception('Sorry, we need to develop code for your type of query (' . get_class($this) . ' ' . $query->executed_action . ').');
				break;
			}
		}
	}

  function check_for_schema($schema_name)  {
     $query = new query(array('data_source_name' => $this->data_source_name, 'action' => 'select', 'query' => "SHOW DATABASES LIKE '{$schema_name}'"));
     db::q($query);
     return (bool) $query->num_rows;
  }
}

