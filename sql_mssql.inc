<?php

// please note: php.ini: mssql.datetimeconvert = Off DOES NOT WORK. PRODUCES GARBAGE. KNOWN BUG - blame freeTDS.
class sql_mssql extends sql_SQL_Server {

	protected function _get_connection() { // called by get_connection. don't break that flow.
		// todo make persistent connections configurable
		// if(self::$connections[$this->data_source->name] = mssql_pconnect($this->data_source->host . ':' . $this->data_source->port, $this->data_source->username, $this->data_source->get_password())) {
		//if(self::$connections[$this->data_source->name] = mssql_connect($this->data_source->host . ':' . $this->data_source->port, $this->data_source->username, $this->data_source->get_password())) {
		// okay, for best results with freeTDS, we name our source, and let the freeTDS configuration handle location and protocal versions.
		// meaning.. have some lines in /etc/freetds.conf like
		// note charset all caps, and tds version 7.2 is the latest for sue with mssql server 2005. could not find anything later than that even for sql server 2012
		/*
		[pcmenu]
		client charset = UTF-8
		port = 12345
		host = 12.34.56.78
		tds version = 7.2
		*/
		if(self::$connections[$this->data_source->name] = mssql_connect($this->data_source->name, $this->data_source->username, $this->data_source->get_password())) {
			// is this part nec?
			if( ! mssql_select_db($this->data_source->default_schema)) throw new Exception('could not select default schema; ' . $this->data_source->default_schema . '. ' . mssql_get_last_message());
		} else {
			throw new Exception('could not make db connection. check settings. host: ' . $this->data_source->host . ':' . $this->data_source->port . ' username: ' . $this->data_source->username); // no other information is available.
		}
	}

	// since ms is lame enough to make me run second queries to get things like last insert ids, they get a method like this:
	protected function quick_and_dirty_query($str) {
		$connection = self::get_connection();
		$result = mssql_query($str, $connection);
		return mssql_fetch_assoc($result);
	}

	function run(query $query) {
		parent::run($query); // mostly for logging and debugging and mssql cleanup
		if($query->action == 'stored_procedure') return $this->execute_stored_procedure($query);

		// todo handle varchars of greater than 255 chars. this solution has been worked into bass/storepromos/stores.inc
		// http://board.phpbuilder.com/showthread.php?10220757-PHP-amp-MS-SQL-text-truncated

		$connection = self::get_connection();

		if($result = mssql_query($query->query, $connection)) {
			$query->raw_result = $result;
			$query->success = true;
			$query->executed = true;
			$query->num_rows = mssql_num_rows($result);
			return $result;
		} else {
			$query->error_messages[] = mssql_get_last_message();
			$this->_trigger_error($query);
			return false;
		}
	}

	function execute_stored_procedure(query $query) {
		// oh boy. need the name of the stored procedure
		throw new Exception('development not finished. what are we doing here?');
		if(empty($query->stored_procedure_name)) trigger_error('no stored procedure name specced.', E_USER_WARNING);
		$stmt = mssql_init($query->stored_procedure_name);
		foreach($query->stored_procedure_params as $param) {
			mssql_bind($stmt, $param['param_name'], $param['value'], $param['sql_data_type']);
		}
		// Execute the statement
		$result = mssql_execute($stmt);

		$data = array();
		while($row = mssql_fetch_assoc($result)) $data[] = $row;

		// And we can free it like so:
		mssql_free_statement($stmt);

		die('<hr>' . @$something . '<br>Died on line ' . __LINE__ . ' of ' . __FILE__);
		if($result = mssql_query($query->query)) {
			$query->raw_result = $result;
			$query->success = true;
			$query->executed = true;
			return $result;
		} else {
			$query->error_messages[] = mssql_get_last_message();
			$this->_trigger_error($query);
			return false;
		}
	}

	protected function groom_stored_procedure_params($params = array()) {
		// params are set up with lots of information for the mssql driver. we need to convert some values to mssql constants.
		// they should already be named as they need to be except for one: data type. we are going to translate that to mssql constants and add them to params
		foreach($params as $key => $param) {
			// sql data type is derived from the supplied sqldata type http://php.net/manual/en/mssql.constants.php
			switch($param['sql_data_type']) {
				case 'integer':
					$params[$key]['sql_data_type'] = SQLINT4; // aiming large.
					break;
				default:
					throw new Exception('unsupported sql data type.'); // todo cover them all.
					break;
			}
		}

		return $params;

	}

	public function groom_result(query $query) {
		parent::groom_result($query); // sets -executed_action and makes sure we executed
		// okay, if it was a select, that's easy. if it was an insert, grab the new id. if it was an update.... or a delete... return rows affected?
		// the parent function just set executed action, go by that
		if( ! isset($query->result)) {
			switch($query->executed_action) {
				case 'select':
					$data = array();
					while($row = mssql_fetch_assoc($query->raw_result)) {
						$data[] = $row;
					}

					if(count($data) === 0) {
						$query->set_empty_result(); // this does things like NULL vs empty array. it is nicer than the same thing all teh time.
					} else {
						$query->set_results($data); // we have some results
					}
					break;

				case 'update':
				case 'delete':
					$query->set_results(TRUE);// maybe more later
					break;
				case 'CREATE': // pre-written like "SET IDENTITY_INSERT {$table} on"
					$query->set_results(TRUE);
					break;

				case 'SET ID': // pre-written like "SET IDENTITY_INSERT {$table} on"
					$query->set_results(TRUE);
					break;

					// INSERTS handled by parent

				default:
					throw new Exception('Sorry, we need to develop code for your type of query (' . get_class($this) . ' ' . $query->executed_action . ').');
					break;
			}
		}
	}

  function check_for_schema($schema_name)  {
     throw new Exception(__FUNCTION__ . ' undeveloped for ' . get_called_class());
  }

}
