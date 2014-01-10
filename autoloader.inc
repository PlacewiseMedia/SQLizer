<?php

// should probably namespace all this stuff
spl_autoload_register(function($class) {
	$file_to_look_for = __DIR__ . '/' . $class . '.inc';
	if(file_exists($file_to_look_for)) include_once($file_to_look_for);
	// do i need to return something?
});
