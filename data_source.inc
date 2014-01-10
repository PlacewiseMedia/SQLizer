<?php

class data_source {
	public $name;
	public $default_schema; // required in some syntaxes. be on your guard.
	public $type; // one of: mssql_{year} mysql_{version}, pgsql... see the db::get_handler func for details
	public $host;
	public $username;
	public $port;
	private $pass;
	public function __construct($name, $default_schema, $type, $host, $username, $pass, $port = NULL, $options = array()) {
		$this->name = $name;
		$this->default_schema = $default_schema;
		$this->type = $type;
		$this->host = $host;
		$this->username = $username;
		$this->pass = $pass;
		if( ! empty($port) and is_numeric($port)) $this->port = (int) $port;
		if( ! empty($options)) foreach($options as $key => $value) $this->key = $value; // eg pgsql's sslmode or maybe some charsets
	}

	public function get_password() {
		return $this->pass;
	}

	/**
	* returns either the set up default or NULL, so that it can be used in arguments.
	*/
	public function get_default_schema() {
		if(isset($this->default_schema)) return $this->default_schema;
		return NULL;
	}
}