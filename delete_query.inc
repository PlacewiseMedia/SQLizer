<?php

class delete_query extends query {

	function __construct($data_source_name, $data_table_name, $where_clause, $options = array()) {
		parent::__construct(array_merge(array('data_source_name' => $data_source_name, 'action' => 'delete', 'data_table_name' => $data_table_name, 'where' => $where_clause), $options));
	}

}