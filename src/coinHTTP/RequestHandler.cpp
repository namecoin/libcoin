/* -*-c++-*- libcoin - Copyright (C) 2012 Michael Gronager
 *
 * libcoin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * libcoin is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libcoin.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <coinHTTP/RequestHandler.h>
#include <coinHTTP/MimeTypes.h>
#include <coinHTTP/Request.h>
#include <coinHTTP/Method.h>
#include <coinHTTP/RPC.h>

#include <fstream>
#include <sstream>
#include <string>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/bind.hpp>

#include <openssl/buffer.h>
#include <openssl/bio.h>
#include <openssl/evp.h>

using namespace std;
using namespace boost;
using namespace json_spirit;

// This method, "dirty", dirties the memory cache of all get requests to force a reload.
class DirtyDocCache : public Method
{
public:
    DirtyDocCache(RequestHandler& delegate) : _delegate(delegate) {}
    virtual Value operator()(const Array& params, bool fHelp) {
        
        cout << _delegate.getDocCacheStats(2) << flush;
        _delegate.clearDocCache();
        return Value::null;
    }    
private:
    RequestHandler& _delegate;
};

class Help : public Method{
public:
    Help(RequestHandler& delegate) : _delegate(delegate) {}
    virtual Value operator()(const Array& params, bool fHelp) {
        if (fHelp)
            throw runtime_error(
                                "help [command]\n"
                                "List commands, or get help for a command.");
        
        string ret;

        Array list;
        const Methods methods = _delegate.getMethods();
        if (params.size() == 0) {
            for (Methods::const_iterator mi = methods.begin(); mi != methods.end(); ++mi) {
                string name = (*mi).first;
                ret += name + "\n";
            }
        }
        else if (params.size() > 0) {
            string command = params[0].get_str();
            Methods::const_iterator mi = methods.find(command);
            if (mi == methods.end()) { // command not found
                return "help: unknown command:" + command + "\n";
            }
            else {
                try {
                    Array params;
                    Method& cmd = *((*mi).second.get());
                    cmd(params, true);
                }
                catch (Object& err) {
                // Help text is returned in an exception
                    ret = find_value(err, "message").get_str();
                }
            }
        }
        return ret;
    }    
    
private:
    RequestHandler& _delegate;
};


static inline bool is_base64(unsigned char c) {
    return (isalnum(c) || (c == '+') || (c == '/'));
}

Auth::Auth(std::string base64auth) {
    // check that the string is indeed a base64 string    
    if(find_if(base64auth.begin(), base64auth.end(), !boost::bind(is_base64, _1)) == base64auth.end())
        _base64auth = base64auth;
    else
        _base64auth = "";
}

string Auth::username() {
    string user_pass = decode64(_base64auth);
    vector<string> userpass;
    algorithm::split(userpass, user_pass, is_any_of(":"));
    if(userpass.size() > 0)
        return userpass[0];
    else
        return "";
}

string Auth::password() {
    string user_pass = decode64(_base64auth);
    vector<string> userpass;
    algorithm::split(userpass, user_pass, is_any_of(":"));
    if(userpass.size() > 1)
        return userpass[1];
    else
        return "";
}

static const std::string base64_chars = 
"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyz"
"0123456789+/";

string Auth::encode64(string s) {
    std::string ret;
    int i = 0;
    int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];
    unsigned int in_len = s.size();
    unsigned char const* bytes_to_encode = (unsigned char const*)s.c_str();
    while (in_len--) {
        char_array_3[i++] = *(bytes_to_encode++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;
            
            for(i = 0; (i <4) ; i++)
                ret += base64_chars[char_array_4[i]];
            i = 0;
        }
    }
    
    if (i)
        {
        for(j = i; j < 3; j++)
            char_array_3[j] = '\0';
        
        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;
        
        for (j = 0; (j < i + 1); j++)
            ret += base64_chars[char_array_4[j]];
        
        while((i++ < 3))
            ret += '=';
        
        }
    
    return ret;
    
}
string Auth::decode64(string encoded_string) {
    int in_len = encoded_string.size();
    int i = 0;
    int j = 0;
    int in_ = 0;
    unsigned char char_array_4[4], char_array_3[3];
    std::string ret;
    
    while (in_len-- && ( encoded_string[in_] != '=') && is_base64(encoded_string[in_])) {
        char_array_4[i++] = encoded_string[in_]; in_++;
        if (i ==4) {
            for (i = 0; i <4; i++)
                char_array_4[i] = base64_chars.find(char_array_4[i]);
            
            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
            
            for (i = 0; (i < 3); i++)
                ret += char_array_3[i];
            i = 0;
        }
    }
    
    if (i) {
        for (j = i; j <4; j++)
            char_array_4[j] = 0;
        
        for (j = 0; j <4; j++)
            char_array_4[j] = base64_chars.find(char_array_4[j]);
        
        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
        
        for (j = 0; (j < i - 1); j++) ret += char_array_3[j];
    }
    
    return ret;
}

RequestHandler::RequestHandler(const string& doc_root) : _doc_root(doc_root) {
    registerMethod(method_ptr(new DirtyDocCache(*this)));
    registerMethod(method_ptr(new Help(*this)));
}

void RequestHandler::registerMethod(method_ptr method, Auth auth) {
    _methods[method->name()] = method;
    if(!auth.isNone()) _auths[method->name()] = auth;
}

void RequestHandler::unregisterMethod(const string name) {
    _methods.erase(name);
    _auths.erase(name);
}

void RequestHandler::async_exec(const Request& req, Reply& rep, const CompletionHandler& handler) {
    // first check in async_exec is if we need to execute or just to access disk
    // the methods takes precedense, they are installed in a path and and by name
    // valid calls are:
    // POST /myservice HTTP/1.1
    // ...
    // { "jsonrpc" : "2.0", "method" : "sum", "params" : [17, 25] }
    // or
    // GET /myservice/sum?a=17&b=25 HTTP/1.1 - note! not supported
    // GET /myservice/sum?1=17&2=25 HTTP/1.1 - note! not supported 
    // GET /myservice/sum?17&25 HTTP/1.1
    // This means to parse this we need to check if this is a post or a get
    // If it is a GET, we fill in the rpc stuff from the call line - i.e. path, method, we then check if it is an installed method, if so we call it, else we try to do a read from disk, and else we just reply 404
    // If it is a POST, we fill in the rpc call from parsing the content, only then do we do a lookup for method, if found we call it, else 404
    
    try {
        // try to resolve the method
        RPC rpc(req); // this will parse the content / uri and return an RPC object - alternatively it will throw en rpc error object
        
        Methods::iterator m = _methods.find(rpc.method());
        if (m != _methods.end()) {
            // check authorization
            if (_auths.count(rpc.method()) && !_auths[rpc.method()].valid(req.basic_auth())) {
	        throw Reply::unauthorized;
		//throw rpc.response(RPC::internal_error, req.basic_auth());
		//output the actual lowercase version of your auth base64 string
            }
            
            // prepare for deferred execution
            rep.setMethod(m->second.get());
            
            // the method object also has a dispatch handler - we use this through rep.dispatch
            
            rep.dispatch(handler);
            return;
        }
        else if (!req.is_get())
            throw rpc.response(RPC::method_not_found);
        
        // assume that it is a get looking for data
        
        // Request path must be absolute and not contain "..".
        std::string path = req.path();
        if (path.empty() || path[0] != '/' || path.find("..") != string::npos)
            throw Reply::bad_request;
        
        
        // Determine the file extension - if there is no extension, first look up the request an RPC call
        size_t last_slash_pos = path.find_last_of("/");
        size_t last_dot_pos = path.find_last_of(".");
        string extension;
        if (last_dot_pos == string::npos) { // could be an RPC call - do a lookup in methods
            path += "/index.html";
            last_slash_pos = path.find_last_of("/");
            last_dot_pos = path.find_last_of(".");
            if (last_dot_pos > last_slash_pos) {
                extension = path.substr(last_dot_pos + 1);
            }
            
            // First check the cache.
            string& content = _doc_cache[path];
            // We support one simple alternative - if the _doc_root begins with a "<" we assume it is not a
            // path but a html document it selves - then we simple return this!
            if (_doc_root.size() > 0 && _doc_root[0] == '<')
                content = _doc_root;
            
            // Not cached - load and cache it
            if(content.size() == 0) {
                // Open the file to send back.
                string full_path = _doc_root + path;
                ifstream is(full_path.c_str(), ios::in | ios::binary);
                if (is) {
                    char buf[512];
                    while (is.read(buf, sizeof(buf)).gcount() > 0)
                        content.append(buf, is.gcount());
                }
                else {
                    // Check if the requested resource is stored as a collection of files - we encode collections by adding a trailing _ to the extension
                    string collection_name = full_path + "_";
                    ifstream collection(collection_name.c_str(), ios::in | ios::binary);
                    if(!collection) {
                        _doc_cache.erase(path);
                        throw Reply::not_found;
                    }
                    
                    // "collection" points to a set of files that should be concatenated
                    while (!collection.eof()) {
                        string path_name = "";
                        getline(collection, path_name);
                        
                        // Construct file name.
                        string full_path = _doc_root + path.substr(0, last_slash_pos+1) + path_name;
                        ifstream part(full_path.c_str(), ios::in | ios::binary);
                        if (part) {
                            char buf[512];
                            while (part.read(buf, sizeof(buf)).gcount() > 0)
                                content.append(buf, part.gcount());
                        }
                        else
                            cerr << "Encountered no such file: " << full_path << ", In trying to read file collection: " << path << " - Ignoring" << endl;
                    }
                }
            }
            
            // Fill out the reply to be sent to the client.
            rep.setContentAndMime(content, MimeTypes::extension_to_type(extension));
        }
        else
            throw Reply::bad_request;
    }
    catch (Reply::Status status) {
        rep.status(status);
    }
    catch (json_spirit::Object& err) {
        rep.setContentAndMime(write(err), "application/json");
    }
    catch (...) {
        rep.status(Reply::not_found);
    }
    handler();
    return;
}

void RequestHandler::clearDocCache() {
    _doc_cache.clear();
}

std::string RequestHandler::getDocCacheStats(int level) {
    string stats;
    long size = 0;
    int entries = 0;
    ostringstream oss;
    for(DocCache::iterator i = _doc_cache.begin(); i != _doc_cache.end(); ++i) {
        oss << i->first << " : " << i->second.size() << "\n";
        size += i->second.size();
        entries++;
    }
    switch (level) {
        case 2:
            stats.append(oss.str());
        case 1:
            oss.clear();
            oss << "Entries: " << entries << " Total Size: " << size << "\n";
            stats.append(oss.str()); 
            break;
        default:
            oss.clear();
            oss << size;
            stats.append(oss.str()); 
            break;
    }
    return stats;
}
