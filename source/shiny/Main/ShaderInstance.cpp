#include "ShaderInstance.hpp"

#include <stdexcept>

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

#include "Preprocessor.hpp"
#include "Factory.hpp"
#include "ShaderSet.hpp"


namespace sh
{
	ShaderInstance::ShaderInstance (ShaderSet* parent, const std::string& name, PropertySetGet* properties)
		: mName(name)
		, mParent(parent)
		, mSupported(true)
	{
		std::string source = mParent->getSource();
		int type = mParent->getType();
		std::string basePath = mParent->getBasePath();

		// replace properties
		size_t pos;
		while (true)
		{
			pos =  source.find("@shProperty");
			if (pos == std::string::npos)
				break;
			size_t start = source.find("(", pos);
			size_t end = source.find(")", pos);
			std::string cmd = source.substr(pos+1, start-(pos+1));
			std::string replaceValue;
			if (cmd == "shPropertyBool")
			{
				std::string propertyName = source.substr(start+1, end-(start+1));
				PropertyValuePtr value = properties->getProperty(propertyName);
				bool val = retrieveValue<BooleanValue>(value, properties->getContext()).get();
				replaceValue = val ? "1" : "0";
			}
			else if (cmd == "shPropertyString")
			{
				std::string propertyName = source.substr(start+1, end-(start+1));
				PropertyValuePtr value = properties->getProperty(propertyName);
				replaceValue = retrieveValue<StringValue>(value, properties->getContext()).get();
			}
			else if (cmd == "shPropertyEqual")
			{
				size_t comma_start = source.find(",", pos);
				size_t comma_end = comma_start+1;
				// skip spaces
				while (source[comma_end] == ' ')
					++comma_end;
				std::string propertyName = source.substr(start+1, comma_start-(start+1));
				std::string comparedAgainst = source.substr(comma_end, end-comma_end);
				std::string value = retrieveValue<StringValue>(properties->getProperty(propertyName), properties->getContext()).get();
				replaceValue = (value == comparedAgainst) ? "1" : "0";
			}
			else
				throw std::runtime_error ("unknown command \"" + cmd + "\" in \"" + name + "\"");
			source.replace(pos, (end+1)-pos, replaceValue);
		}

		// replace global settings
		while (true)
		{
			pos =  source.find("@shGlobalSetting");
			if (pos == std::string::npos)
				break;

			size_t start = source.find("(", pos);
			size_t end = source.find(")", pos);
			std::string cmd = source.substr(pos+1, start-(pos+1));
			std::string replaceValue;
			if (cmd == "shGlobalSettingBool")
			{
				std::string settingName = source.substr(start+1, end-(start+1));
				std::string value = mParent->getCurrentGlobalSettings()->find(settingName)->second;
				replaceValue = (value == "true" || value == "1") ? "1" : "0";
			}
			if (cmd == "shGlobalSettingEqual")
			{
				size_t comma_start = source.find(",", pos);
				size_t comma_end = comma_start+1;
				// skip spaces
				while (source[comma_end] == ' ')
					++comma_end;
				std::string settingName = source.substr(start+1, comma_start-(start+1));
				std::string comparedAgainst = source.substr(comma_end, end-comma_end);
				std::string value = mParent->getCurrentGlobalSettings()->find(settingName)->second;
				replaceValue = (value == comparedAgainst) ? "1" : "0";
			}

			else
				throw std::runtime_error ("unknown command \"" + cmd + "\" in \"" + name + "\"");
			source.replace(pos, (end+1)-pos, replaceValue);
		}

		// parse foreach
		while (true)
		{
			pos = source.find("@shForeach");
			if (pos == std::string::npos)
				break;

			size_t start = source.find("(", pos);
			size_t end = source.find(")", pos);
			int num = boost::lexical_cast<int>(source.substr(start+1, end-(start+1)));

			assert(source.find("@shEndForeach", pos) != std::string::npos);
			size_t block_end = source.find("@shEndForeach", pos);

			// get the content of the inner block
			std::string content = source.substr(end+1, block_end - (end+1));

			// replace both outer and inner block with content of inner block num times
			std::string replaceStr;
			for (int i=0; i<num; ++i)
			{
				// replace @shIterator with the current iteration
				std::string addStr = content;
				boost::replace_all(addStr, "@shIteration", boost::lexical_cast<std::string>(i));
				replaceStr += addStr;
			}
			source.replace(pos, (block_end+std::string("@shEndForeach").length())-pos, replaceStr);
		}

		// why do we need our own preprocessor? there are several custom commands available in the shader files
		// (for example for binding uniforms to properties or auto constants) - more below. it is important that these
		// commands are _only executed if the specific code path actually "survives" the compilation.
		// thus, we run the code through a preprocessor first to remove the parts that are unused because of
		// unmet #if conditions (or other preprocessor directives).
		Preprocessor p;
		std::string newSource = p.preprocess(source, basePath, std::vector<std::string>(), name);

		Platform* platform = Factory::getInstance().getPlatform();

		if (type == ShaderSet::Type_Vertex)
			mProgram = boost::shared_ptr<Program>(platform->createVertexProgram("", mName, mParent->getProfile(), newSource, Factory::getInstance().getCurrentLanguage()));
		else if (type == ShaderSet::Type_Fragment)
			mProgram = boost::shared_ptr<Program>(platform->createFragmentProgram("", mName, mParent->getProfile(), newSource, Factory::getInstance().getCurrentLanguage()));

		if (!mProgram->getSupported())
			mSupported = false;
	}

	std::string ShaderInstance::getName ()
	{
		return mName;
	}

	bool ShaderInstance::getSupported () const
	{
		return mSupported;
	}
}