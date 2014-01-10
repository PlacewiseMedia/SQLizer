<?php

class select_query extends query {

	function __construct($data_source_name, $fields = '*', $from = NULL, $where = NULL, $options = array()) {
		$args = array_merge(array('data_source_name' => $data_source_name, 'action' => 'select', 'from' => $from, 'select' => $fields, 'where' => $where), $options);
		parent::__construct(array_merge(array('data_source_name' => $data_source_name, 'action' => 'select', 'from' => $from, 'select' => $fields, 'where' => $where), $options));
	}
}