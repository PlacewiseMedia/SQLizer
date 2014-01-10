<?php

class sql_sqlsrv extends sql_SQL_Server {

	protected function _get_connection() { // called by get_connection. don't break that flow.
		if(empty($this->data_source->default_schema)) throw new Exception('SQL Server requires a default database be specified for connections. Please set on up in your config.');
		$options = array(
			'Database' => $this->data_source->default_schema, // see? kinda default, kinda not.
			// 'ReturnDatesAsStrings'=> true,
			'CharacterSet' => 'UTF-8',
			'ConnectionPooling' => false
		);

		// not having username and pass will force windows auth.
		if(isset($this->data_source->username) and strlen($this->data_source->username)) {
			$options['UID'] = $this->data_source->username;
			$options['PWD'] = $this->data_source->get_password();
		}

		if( ! self::$connections[$this->data_source->name] = sqlsrv_connect($this->data_source->host . ', ' . $this->data_source->port, $options)) {
			throw new Exception('cannot connect sqlsrv - ' . implode(' | ', $this->gather_errors()));
		}
	}

	function gather_errors() {
		$ret = array();
		foreach(sqlsrv_errors() as $error) if(isset($error['message'])) $ret[] = $error['message'];
			return $ret;
	}

	function _trigger_error(query $query) {
		$query->error_messages = array_merge($query->error_messages, $this->gather_errors());
		parent::_trigger_error($query);
	}

	function run(query $query) {
		parent::run($query); // mostly for logging and debugging and mssql cleanup - also stored procedure param cleanup
		// do we have a connection?
		$connection = self::get_connection();
		// http://msdn.microsoft.com/en-us/library/hh487160.aspx
		// we started with SQLSRV_CURSOR_STATIC - which worked until we needed stored procedures.
		// now we are trying SQLSRV_CURSOR_CLIENT_BUFFERED, which is probably what it should have been all along. 2013 09 ab
		if($result = sqlsrv_query($connection, $query->query, $query->stored_procedure_params, array('Scrollable' => SQLSRV_CURSOR_CLIENT_BUFFERED))) { // todo: taht scrollable thing is weird. need static or keyset so taht we can get numrows. shrug
			$query->raw_result = $result;
			$query->success = true;
			$query->executed = true;
			$query->num_rows = sqlsrv_num_rows($result);
			return $result;
		} else {
			// sqlsrv_errors() returns an array of arrays of lots of info.
			$temp = sqlsrv_errors();
			if(m::help('sql')) m::dump($temp, 'ERRORS on this query: ' . $query->query); // normally this is done solely in the _trigger_error, but there's all this extra info....
			foreach($temp as $error) if(isset($error['message'])) $query->error_messages[] = $error['message'];
			$this->_trigger_error($query);
			return false;
		}
	}

	protected function groom_stored_procedure_params($params = array()) {
		// params are set up with lots of information for the mssql driver. we only need a couple things, so we're going to turn it directly into the array we need.
		// note that this discards some data about the params.
		$ret = array();
		foreach($params as $param) {
			if( ! isset($param['direction'])) $param['direction'] = 'in'; // same default as driver
			// value is easy, start with that:
			$temp = array($param['value']);
			// direction can be one of SQLSRV_PARAM_IN, SQLSRV_PARAM_OUT, SQLSRV_PARAM_INOUT.
			switch($param['direction']) {
				case 'out':
					$temp[] = SQLSRV_PARAM_OUT;
					break;
				case 'inout':
					$temp[] = SQLSRV_PARAM_INOUT;
					break;
				default: //  'in'; // same default as driver
					$temp[] = SQLSRV_PARAM_IN;
					break;
			}
			//optional, not developed: php data type and sql data type
			$ret[] = $temp;
		}

		return $ret;

	}

	public function groom_result(query $query) {
		parent::groom_result($query); // sets -executed_action and makes sure we executed
		// okay, if it was a select, that's easy. if it was an insert, grab the new id. if it was an update.... or a delete... return rows affected?
		// the parent function just set executed action, go by that (unless it also set the result)

		if( ! isset($query->result)) {
			switch($query->executed_action) {
				case 'select':
					$data = array();
					while($row = sqlsrv_fetch_array($query->raw_result, SQLSRV_FETCH_ASSOC)) $data[] = $row;

					// sqlsrv pulls datetime objects out of the db instead of strings. convert for legacy usage in bass.
					// todo - allow disabling of this per query - dates_as_objects, dates_as_strings, date_formats.
					foreach($data as $key => $record) {
						foreach($record as $field_name => $value) {
							if(is_object($value)) {
								switch(get_class($value)) { // i put this as a switch in case another data type had issues. if this is the only one, you could save some cycles with an if.
									case 'DateTime':
										$data[$key][$field_name] = $value->format('Y-m-d H:i:s');
										break;
								}
							}
						}
					}

					if(count($data) === 0) {
						$query->set_empty_result(); // this does things like NULL vs empty array. it is nicer than the same thing all teh time.
					} else {
						$query->set_results($data); // we have some results
					}
					break;

				case 'update':
				case 'delete':
					$query->set_results(TRUE);
					$query->rows_matched = sqlsrv_rows_affected($query->raw_result); //not sure why this doesnt work
					break;
				case 'SET ID': // pre-written like "SET IDENTITY_INSERT {$table} on"
					$query->set_results(TRUE);
					break;
				case 'CREATE': // pre-written
					$query->set_results(TRUE);
					break;
					// INSERTs are handled by parent. case 'insert':
				default:
					throw new Exception('Sorry, we need to develop code for your type of query (' . get_class($this) . ' ' . $query->executed_action . ').');
					break;
			}
		}
	}

	// since ms is lame enough to make me run second queries to get things like last insert ids, they get a method like this:
	protected function quick_and_dirty_query($str) {
		$connection = self::get_connection();
		$result = sqlsrv_query($connection, $str, null, array('Scrollable' => SQLSRV_CURSOR_CLIENT_BUFFERED));
		//\m::death(sqlsrv_fetch_array($result, SQLSRV_FETCH_ASSOC));
		return sqlsrv_fetch_array($result, SQLSRV_FETCH_ASSOC);
	}

  function check_for_schema($schema_name)  {
     throw new Exception(__FUNCTION__ . ' undeveloped for ' . get_called_class());
  }

}