arguments = argv();

first_filename = arguments{1}%"original_output"
second_filename = arguments{2}%"tr_transparent_with_queue"

fid_one = fopen(first_filename, "r");
fid_two = fopen(second_filename, "r");

for i = 1:50
	stuff = fread(fid_one, 1, "float", "native");
	stuff_2 = fread(fid_two, 1, "float", "native");
	disp(stuff), disp(stuff_2);
endfor
