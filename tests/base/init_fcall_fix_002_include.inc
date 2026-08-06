<?php

function dummy_func(string $value): string {
	return 'worker-' . $value . getenv('DUMMY_VALUE');
}
