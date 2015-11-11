function f0(a, b)
	print("f0".."a="..a..", b="..b);
end

function f1(a, b)
	print("f1".."a="..a..", b="..b);
	return a + b;
end

function f2(a, b)
	print("f2".."a="..a..", b="..b);
	return a + 1, b + 1;
end

