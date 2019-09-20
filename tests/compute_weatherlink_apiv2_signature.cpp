#include <iostream>
#include <map>

#include "../src/http_utils.h"
#include "../src/davis/weatherlink_apiv2_downloader.h"

using namespace meteodata;

int main()
{
	std::map<std::string, std::string> params{
		{"t", "1568718072"},
		{"api-key", "987654321"},
		{"station-id", "2"}
	};

	std::string allParamsButApiSignature;
	for (const auto& param : params)
		allParamsButApiSignature += param.first + param.second;


	std::cout << "Hashed string is: \"" << allParamsButApiSignature << "\"" << std::endl;

	std::string str = "api-key987654321station-id2t1568718072";
	std::string key = "ABC123";

	std::cout << "Is the string built from the params correct? " << std::boolalpha << (str == allParamsButApiSignature) << std::endl;
	std::cout << "API signature is: \"" << std::hex << computeHMACWithSHA256(str, key) << "\"" << std::endl;

	params = {
		{"station-id", "72443"},
		{"api-key", "987654321"},
		{"t", "1562176956"},
		{"start-timestamp", "1561964400"},
		{"end-timestamp", "1562050800"}
	};

	allParamsButApiSignature = "";
	for (const auto& param : params)
		allParamsButApiSignature += param.first + param.second;


	std::cout << "Hashed string is: \"" << allParamsButApiSignature << "\"" << std::endl;

	str = "api-key987654321end-timestamp1562050800start-timestamp1561964400station-id72443t1562176956";

	std::cout << "Is the string built from the params correct? " << std::boolalpha << (str == allParamsButApiSignature) << std::endl;
	std::cout << "API signature is: \"" << std::hex << computeHMACWithSHA256(str, key) << "\"" << std::endl;

}
