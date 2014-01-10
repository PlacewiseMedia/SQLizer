<?php

class insert_query extends query {

	function __construct($data_source_name, $data_table_name, $data = array(), $options = array()) {
		parent::__construct(array_merge(array('data_source_name' => $data_source_name, 'action' => 'insert', 'data' => $data, 'data_table_name' => $data_table_name), $options));
	}

}