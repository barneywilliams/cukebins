#ifndef CUKEBINS_WIREPROTOCOL_HPP_
#define CUKEBINS_WIREPROTOCOL_HPP_

#include <json_spirit/json_spirit_reader_template.h>
#include <json_spirit/json_spirit_writer_template.h>
#include <cukebins/internal/step/StepManager.hpp>
#include <cukebins/internal/CukeCommands.hpp>

#include <string>
#include <fstream>
#include <boost/shared_ptr.hpp>
#include <boost/assign/list_of.hpp>

namespace cukebins {
namespace internal {

using json_spirit::mValue;
using json_spirit::mArray;
using json_spirit::mObject;
using std::make_pair;

class NetworkProtocol {
public:
    virtual void processStream(std::iostream &stream) = 0;
};

class JSONCommand {
public:
    virtual mValue run(mValue &jsonArgs) = 0;
    static mValue success_response();
    static mValue fail_response();

protected:
    CukeCommands commands;
};

class BeginScenarioCommand : public JSONCommand {
public:
    mValue run(mValue &jsonArgs);
};

class EndScenarioCommand : public JSONCommand {
public:
    mValue run(mValue &jsonArgs);
};

class SnippetTextCommand : public JSONCommand {
public:
    mValue run(mValue &jsonArgs);
private:
    mValue formatResponse(const std::string snippetText);
};

class StepMatchesCommand : public JSONCommand {
public:
    mValue run(mValue &jsonArgs);
private:
    std::string getStepMatchesMatcher(mValue &jsonArgs);
    mValue formatResponse(MatchResult matchResult);
};

class InvokeCommand : public JSONCommand {
public:
    mValue run(mValue &jsonArgs);
private:
    step_id_type getInvokeId(mValue &jsonArgs);
    const InvokeArgs *getInvokeArgs(mValue &jsonArgs);
    void fillTableArg(Table & tableArg, const mArray &tableArray);
    mValue formatResponse(const InvokeResult &result);
};

class WireProtocol : public NetworkProtocol {
public:
    void processStream(std::iostream &stream);
    void processOneRequest(std::iostream &stream);
private:
    mValue invokeCommand(mValue &decodedRequestValue);

    CukeCommands commands;
    static std::map<std::string, boost::shared_ptr<JSONCommand> > jsonCommands;
};

std::map<std::string, boost::shared_ptr<JSONCommand> > WireProtocol::jsonCommands = boost::assign::map_list_of
    ("begin_scenario", shared_ptr<JSONCommand> (new BeginScenarioCommand))
    ("end_scenario", shared_ptr<JSONCommand> (new EndScenarioCommand))
    ("step_matches", shared_ptr<JSONCommand> (new StepMatchesCommand))
    ("invoke",shared_ptr<JSONCommand> (new InvokeCommand))
    ("snippet_text",shared_ptr<JSONCommand> (new SnippetTextCommand));


mValue JSONCommand::success_response() {
    mArray a;
    a.push_back("success");
    return mValue(a);
}

mValue JSONCommand::fail_response() {
    mArray a;
    a.push_back("fail");
    return mValue(a);
}

mValue BeginScenarioCommand::run(mValue &jsonArgs) {
    TagExpression::tag_list *tags = new TagExpression::tag_list;
    if (!jsonArgs.is_null()) {
        mArray &jsonTags = jsonArgs.get_obj()["tags"].get_array();
        for (mArray::const_iterator i = jsonTags.begin(); i != jsonTags.end(); ++i) {
            std::string tag = i->get_str();
            tags->push_back(tag);
        }
    }
    commands.beginScenario(tags);
    return success_response();
}

mValue EndScenarioCommand::run(mValue &jsonArgs) {
    if (jsonArgs.is_null() || !jsonArgs.get_obj()["tags"].is_null()) {
        commands.endScenario();
        return success_response();
    } else {
        return fail_response();
    }
}

mValue StepMatchesCommand::run(mValue &jsonArgs) {
    std::string matcher = getStepMatchesMatcher(jsonArgs);
    return formatResponse(commands.stepMatches(matcher));
}

std::string StepMatchesCommand::getStepMatchesMatcher(mValue &jsonArgs) {
    mObject args = jsonArgs.get_obj();
    std::string matcher(args["name_to_match"].get_str());
    return matcher;
}

mValue StepMatchesCommand::formatResponse(MatchResult matchResult) {
    mValue response = success_response();
    mArray matches;
    MatchResult::match_results_type resultSet = matchResult.getResultSet();
    for (MatchResult::match_results_type::iterator i = resultSet.begin(); i != resultSet.end(); ++i) {
        mObject match;
        const StepInfo *stepInfo = i->stepInfo;
        match.insert(make_pair("id", mValue(toString(stepInfo->id))));
        mArray matchArgs;
        for (SingleStepMatch::submatches_type::iterator j = i->submatches.begin(); j != i->submatches.end(); ++j) {
            mObject arg;
            arg.insert(make_pair("val", mValue(j->value)));
            arg.insert(make_pair("pos", mValue(j->position)));
            matchArgs.push_back(arg);
        }
        match.insert(make_pair("args", mValue(matchArgs)));
        match.insert(make_pair("source", mValue(stepInfo->source)));
        matches.push_back(match);
    }
    response.get_array().push_back(matches);
    return response;
}


mValue SnippetTextCommand::run(mValue &jsonArgs) {
    mObject args = jsonArgs.get_obj();
    const std::string stepKeyword(args["step_keyword"].get_str());
    const std::string stepName(args["step_name"].get_str());
    return formatResponse(commands.snippetText(stepKeyword, stepName));
}

mValue SnippetTextCommand::formatResponse(const std::string snippetText) {
    mValue response = success_response();
    response.get_array().push_back(snippetText);
    return response;
}


mValue InvokeCommand::run(mValue &jsonArgs) {
    step_id_type id = getInvokeId(jsonArgs);
    shared_ptr<const InvokeArgs> spArgs(getInvokeArgs(jsonArgs));
    return formatResponse(commands.invoke(id, spArgs.get()));
}

step_id_type InvokeCommand::getInvokeId(mValue &jsonArgs) {
    std::string idString(jsonArgs.get_obj()["id"].get_str());
    return fromString<step_id_type> (idString);
}

const InvokeArgs *InvokeCommand::getInvokeArgs(mValue &jsonArgs) {
    InvokeArgs *args = new InvokeArgs;
    const mArray &argsArray = jsonArgs.get_obj()["args"].get_array();
    for (mArray::const_iterator i = argsArray.begin(); i != argsArray.end(); ++i) {
        try {
            const std::string arg = i->get_str();
            args->addArg(arg);
        } catch (...) {
            fillTableArg(args->getVariableTableArg(), i->get_array());
        }
    }
    return args;
}

void InvokeCommand::fillTableArg(Table & tableArg, const mArray &tableArray) {
    for (mArray::const_iterator i = tableArray.begin(); i != tableArray.end(); ++i) {
        const mArray rowArray = i->get_array();
        if (i == tableArray.begin()) { // the first row are the columns
            for (mArray::const_iterator j = rowArray.begin(); j != rowArray.end(); ++j) {
                tableArg.addColumn(j->get_str());
            }
        } else {
            Table::row_type row;
            for (mArray::const_iterator j = rowArray.begin(); j != rowArray.end(); ++j) {
                row.push_back(j->get_str());
            }
            tableArg.addRow(row);
        }
    }
}

mValue InvokeCommand::formatResponse(const InvokeResult &result) {
    mArray a;
    switch (result.getType()) {
    case SUCCESS:
        a.push_back("success");
        break;
    case PENDING:
        a.push_back("pending");
        if (!result.getDescription().empty()) {
            a.push_back(result.getDescription());
        }
        break;
    case FAILURE:
        mObject failSpecs;
        a.push_back("fail");
        failSpecs.insert(make_pair("message", mValue(result.getDescription())));
        a.push_back(failSpecs);
        break;
    }
    return mValue(a);
}

void WireProtocol::processStream(std::iostream &stream) {
    std::string request;
    while (stream) {
        processOneRequest(stream);
    }
}

void WireProtocol::processOneRequest(std::iostream &stream) {
    mValue decodedRequestValue;
    read_stream(stream, decodedRequestValue);
    if (decodedRequestValue.is_null()) {
        return;
    }
    mValue response;
    try {
        response = invokeCommand(decodedRequestValue);
    } catch (...) {
        response = JSONCommand::fail_response();
    }
    json_spirit::write_stream(response, stream, false);
    stream << std::endl << std::flush;
}
    
mValue WireProtocol::invokeCommand(mValue &decodedRequestValue) {
    mArray decodedRequest = decodedRequestValue.get_array();
    if (decodedRequest.size() > 0) {
        std::string commandName = decodedRequest[0].get_str();
        JSONCommand *requestedCommand = jsonCommands[commandName].get();
        if (requestedCommand != 0) {
            mValue args;
            if (decodedRequest.size() > 1) {
                args = decodedRequest[1];
            }
            return requestedCommand->run(args);
        }
    }
    return JSONCommand::fail_response();
}


}
}

#endif /* CUKEBINS_WIREPROTOCOL_HPP_ */
