<?php

/*
handlers are objects that handle a specific type of sql, with a specific set of php functions, eg mssql with mssql_*() or mssql with sqlsrv_*().
each handler will have one or more connections that it uses to different dbs that it handles, eg db_pimps or db_hoes.

todo implement caching
*/
final class db {
	private static $data_sources; // array of data_source objects
	private static $config;
	private static $handlers = array(); // handlers handle syntax types - mysql vs mssql. each handler has it's own set of connections - eg db_pimps or db_hoes.

	private function __construct() {}

    public static function init() {
         if(func_num_args()) foreach(func_get_args() as $arg) {
             // if it is an array, recurse
             if(is_array($arg)) {
                 foreach($arg as $sub_arg) self::init($sub_arg);
             } elseif(is_object($arg) and $arg instanceof data_source) {
                 static::add_data_source($arg);
             } else {
                 throw new Exception('what should I do with your init args? I am expecting a data_source or an array of them.');
             }
         }
     }

	public static function get_handler_by_name($data_source_name) {
		if(isset(self::$data_sources[$data_source_name])) return self::get_handler(self::$data_sources[$data_source_name]);
		$exception = new Exception('unknown datasource: ' . $data_source_name);
		$exception->debug_data = debug_backtrace(); // ???
		throw $exception;
	}

	/**
	* internal use only. it gets you the object that will handle a particular syntax, such as mysql or tsql.
	*
	* @param stdClass $data_source - straight from config, please.
	* @return sql_*
	*/
	public static function get_handler(data_source $data_source) { // todo public? private?
		if(isset(self::$handlers[$data_source->name])) return self::$handlers[$data_source->name];

		// whelp,.... better go make one.
		if(stripos($data_source->type, 'mssql') !== false) {

			// mssql t-sql type stuff
			if(extension_loaded('sqlsrv')) { // windows
				self::$handlers[$data_source->name] = new sql_sqlsrv($data_source);
			} elseif(extension_loaded('mssql')) { // *nix don't forget freetds.org
				self::$handlers[$data_source->name] = new sql_mssql($data_source);
			} else {
				throw new Exception("cannot handle {$data_source->type}");
			}

		// postgres
		} elseif(stripos($data_source->type, 'pg') !== false or stripos($data_source->type, 'post') !== false) {
			self::$handlers[$data_source->name] = new sql_pg($data_source);

		} else { // defaulting to mysqli - mysql is depped.
			if(extension_loaded('mysqli')) {
				self::$handlers[$data_source->name] = new sql_mysqli($data_source);
			} else {
				throw new Exception("cannot handle {$data_source->type}");
			}
		}

		return self::$handlers[$data_source->name];

	}

	protected static function add_data_source(data_source $data_source) {
		static::$data_sources[$data_source->name] = $data_source;
	}

  public static function get_data_source($data_source_name) {
    if(isset(self::$data_sources[$data_source_name])) return self::$data_sources[$data_source_name];
    return false;
  }

	public static function get_data_sources() {
		return self::$data_sources;
	}

	// todo handle errors better
  public static function check_for_schema($data_source_name, $schema_name) {
    if( ! $data_source = self::get_data_source($data_source_name)) return false;
    if( ! $handler = self::get_handler($data_source)) return false;
    return $handler->check_for_schema($schema_name);
  }

	/**
	* returns data formatted to your specifications, or false on error.
	* accepts only query objects - and since it is an object, it will be changed when you are done.
	* @param query $query
	*/
	static function q(query $query) {
		// get the handler, which handles a particular syntax.
		$handler = self::get_handler_by_name($query->data_source_name); // this will eventually make a db connection
		// easy peasy
		if($result = $handler->run($query)) {
			// success - convert result to final format - set $query->result
			$handler->groom_result($query);
			if(m::help('sql', 9)) m::dump($query, 'ran this query');
			return $query->result;
		} else {
			// the handler will have already triggered an error. we don't do that here. just return false. the code surrounding db::q() should handle a bad result.
			return false;
		}
	}

}



