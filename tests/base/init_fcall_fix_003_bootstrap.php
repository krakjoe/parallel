<?php

function dummy_func(string $value): string {
	return 'bootstrap-' . $value . getenv('DUMMY_VALUE');
}
