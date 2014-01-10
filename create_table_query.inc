<?php
/**
 * User: Alex Brown
 * Date: 7/14/13
 * Time: 9:53 AM
 */

 // not finished
class create_table_query extends query {
    function __construct($data_source_name, $data_table_name, $data = array(), $where_clause, $options = array()) {
        //blah
        parent::__construct(array_merge(array('data_source_name' => $data_source_name, 'action' => 'update', 'data' => $data, 'data_table_name' => $data_table_name, 'where' => $where_clause), $options));
    }
}