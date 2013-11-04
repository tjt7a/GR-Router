fid_python = fopen("python.out", "r");
fid_cpp = fopen("cpp.out", "r");

# Get values for python
temp = fgetl(fid_python);
python_values = [];
i = 1;
while temp != -1
	python_values(i++) = str2num(temp);
	temp = fgetl(fid_python);
endwhile
fclose(fid_python);

# Get values for c++
temp = fgetl(fid_cpp);
cpp_values = [];
i = 1;
while temp != -1
	cpp_values(i++) = str2num(temp);
	temp = fgetl(fid_cpp);
endwhile

fclose(fid_cpp);

x = 1 : 1 : 1024;

subplot(2, 1, 1)
plot(x, python_values, x, cpp_values)
title("Python and C++ Blackmanharris Windows")

differences = python_values - cpp_values;

subplot(2, 1, 2)
plot(x, differences);
title("Difference Between Python and C++ Blackmanharris Windows")
pause