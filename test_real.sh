
make

gnome-terminal -- ./server
sleep 1
gnome-terminal -- erl -pa job-scheduler/compiled_code/ -eval 'job_scheduler:scheduler_init(12000).' -no-shell

