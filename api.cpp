#include "curl_header.h"
#include "curl_ios.h"

#include "api.h"

#include <numeric>
#include <iterator>
#include <string>

#include <json/json.h>

#define NV_PAIR(name, value) curl_pair<CURLformoption, string>(CURLFORM_COPYNAME, name), \
                             curl_pair<CURLformoption, string>(CURLFORM_COPYCONTENTS, value.c_str())

#define NV_FILEPAIR(name, value) curl_pair<CURLformoption, string>(CURLFORM_COPYNAME, name), \
    curl_pair<CURLformoption, string>(CURLFORM_FILE, value.c_str())

static const string SAFE_USER_AGENT = "Mozilla / 5.0(Windows; U; Windows NT 5.1; en - US; rv: 1.9.0.1) Gecko / 2008070208 Firefox / 3.0.1";

static const string AUTH_DOMAIN = "https://auth.mail.ru";
static const string CLOUD_DOMAIN = "https://cloud.mail.ru";

static const string AUTH_ENDPOINT = AUTH_DOMAIN + "/cgi-bin/auth";
static const string SCLD_COOKIE_ENDPOINT = AUTH_DOMAIN + "/sdc";

static const string SCLD_SHARD_ENDPOINT = CLOUD_DOMAIN + "/api/v2/dispatcher";
static const string SCLD_TOKEN_ENDPOINT = CLOUD_DOMAIN + "/api/v2/tokens/csrf";
static const string SCLD_ADDFILE_ENDPOINT = CLOUD_DOMAIN + "/api/v2/file/add";
static const string SCLD_ADDFOLDER_ENDPOINT = CLOUD_DOMAIN + "/api/v2/folder/add";

static const long MAX_FILE_SIZE = 2L * 1000L * 1000L * 1000L;

static void dump(const char *text, FILE *stream, unsigned char *ptr, size_t size, char nohex)
{
  size_t i;
  size_t c;

  unsigned int width=0x10;

  if(nohex)
    /* without the hex output, we can fit more on screen */
    width = 0x80;

  fprintf(stream, "%s, %10.10ld bytes (0x%8.8lx)\n", text, (long)size, (long)size);

  for(i = 0; i < size; i += width) {

    fprintf(stream, "%4.4lx: ", (long)i);

    if(!nohex) {
      /* hex not disabled, show it */
      for(c = 0; c < width; c++)
        if(i+c < size)
          fprintf(stream, "%02x ", ptr[i+c]);
        else
          fputs("   ", stream);
    }

    for(c = 0; (c < width) && (i+c < size); c++) {
      /* check for 0D0A; if found, skip past and start a new line of output */
      if(nohex && (i + c + 1 < size) && ptr[i + c] == 0x0D && ptr[i + c + 1] == 0x0A) {
        i += (c + 2 - width);
        break;
      }
      fprintf(stream, "%c",
              (ptr[i+c]>=0x20) && (ptr[i+c]<0x80)?ptr[i+c]:'.');
      /* check again for 0D0A, to avoid an extra \n if it's at width */
      if(nohex && (i + c + 2 < size) && ptr[i + c + 1] == 0x0D && ptr[i + c + 2] == 0x0A) {
        i += (c + 3 - width);
        break;
      }
    }
    fputc('\n', stream); /* newline */
  }
  fflush(stream);
}

static int trace_post(CURL *handle, curl_infotype type, char *data, size_t size, void *userp)
{
  const char *text;
  (void) handle; /* prevent compiler warning */
  (void) userp;

  switch (type) {
  case CURLINFO_TEXT:
    fprintf(stderr, "== Info: %s", data);
  default: /* in case a new one is introduced to shock us */
    return 0;

  case CURLINFO_HEADER_OUT:
    text = "=> Send header";
    break;
  case CURLINFO_DATA_OUT:
    text = "=> Send data";
    break;
  case CURLINFO_SSL_DATA_OUT:
    text = "=> Send SSL data";
    break;
  case CURLINFO_HEADER_IN:
    text = "<= Recv header";
    break;
  case CURLINFO_DATA_IN:
    text = "<= Recv data";
    break;
  case CURLINFO_SSL_DATA_IN:
    text = "<= Recv SSL data";
    break;
  }

  dump(text, stderr, (unsigned char *)data, size, 1);
  return 0;
}

struct MailApiException : public exception {
    MailApiException(string reason) : reason(reason) {}

    string reason;

    virtual const char *what() const noexcept override;
};

const char *MailApiException::what() const noexcept
{
    return reason.c_str();
}

string API::paramString(Params const &params)
{
    if(params.empty())
        return "";

    vector<string> result;
    result.reserve(params.size());
    for (auto it = params.cbegin(); it != params.end(); ++it) {
        string name = it->first, value = it->second;
        if (name.empty())
            continue;

        m_client->escape(name);
        m_client->escape(value);
        string argument = value.empty() ? name : name + "=" + value;
        result.push_back(argument);
    }

    stringstream s;
    copy(result.cbegin(), result.cend(), ostream_iterator<string>(s, "&"));
    return s.str();
}

string API::performPost(bool reset)
{
    ostringstream stream;
    curl_ios<ostringstream> writer(stream);

    m_client->add<CURLOPT_WRITEFUNCTION>(writer.get_function());
    m_client->add<CURLOPT_WRITEDATA>(writer.get_stream());
    try {
        m_client->perform();
    } catch (curl::curl_easy_exception error) {
        curl::curlcpp_traceback errors = error.get_traceback();
        error.print_traceback();
        throw MailApiException("Couldn't perform request!");
    }
    int64_t ret = m_client->get_info<CURLINFO_RESPONSE_CODE>().get();
    if (ret != 302 && ret != 200) // OK or redirect
        throw MailApiException("non-success return code!");

    if (reset)
        m_client->reset();

    return stream.str();
}

API::API()
    : m_client(make_unique<curl::curl_easy>()),
      m_cookies(*m_client)
{
    m_cookies.set_file(""); // init cookie engine
}

bool API::login(const Account &acc)
{
    if (acc.login.empty())
        return false;

    if (acc.password.empty())
        return false;

    if (!authenticate(acc))
        return false;

    if (!obtainCloudCookie())
        return false;

    if (!obtainAuthToken())
        return false;

    m_acc = acc;
    return true;
}

/**
 * @brief API::authenticate - retrieves initial authentication cookies
 * @return true if cookies were successfully set, false otherwise
 */
bool API::authenticate(const Account &acc)
{
    // Login={0}&Domain={1}&Password={2}

    curl_form form;
    form.add(NV_PAIR("Login", acc.login));
    form.add(NV_PAIR("Password", acc.password));
    form.add(NV_PAIR("Domain", string("mail.ru")));

    m_client->add<CURLOPT_URL>(AUTH_ENDPOINT.data());
    m_client->add<CURLOPT_HTTPPOST>(form.get());
    performPost();

    if (m_cookies.get().empty()) // no cookies received, halt
        return false;

    return true;
}

/**
 * @brief API::obtainCloudCookie - retrieves basic cloud cookie that is needed for API exchange
 * @return true if cookie was successfully obtained, false otherwise
 */
bool API::obtainCloudCookie()
{
    curl_form form;
    form.add(NV_PAIR("from", string(CLOUD_DOMAIN + "/home")));

    size_t cookies_size = m_cookies.get().size();

    m_client->add<CURLOPT_URL>(SCLD_COOKIE_ENDPOINT.data());
    m_client->add<CURLOPT_HTTPPOST>(form.get());
    m_client->add<CURLOPT_FOLLOWLOCATION>(1L);
    performPost();

    if (m_cookies.get().size() <= cookies_size) // didn't get any new cookies
        return false;

    return true;
}

/**
 * @brief API::obtainAuthToken - retrieve auth token. This is the first step in Mail.ru cloud API exchange
 * @return true if token was obtained, false otherwise
 */
bool API::obtainAuthToken()
{
    using Json::Value;

    curl_header header;
    header.add("Accept: application/json");
    
    m_client->add<CURLOPT_USERAGENT>(SAFE_USER_AGENT.data()); // 403 without this
    m_client->add<CURLOPT_HTTPHEADER>(header.get());
    m_client->add<CURLOPT_URL>(SCLD_TOKEN_ENDPOINT.c_str());
    string answer = performPost();

    Value response;
    Json::Reader reader;

    if (!reader.parse(answer, response)) // invalid JSON (shouldn't happen)
        return false;

    if (response["body"] != Value() && response["body"]["token"] != Value()) {
        m_token = response["body"]["token"].asString();
        return true;
    }

    return false;
}

Shard API::obtainShard(Shard::ShardType type)
{
    using Json::Value;

    curl_header header;
    header.add("Accept: application/json");

    string url = SCLD_SHARD_ENDPOINT + "?" + paramString({{"token", m_token}});
    m_client->add<CURLOPT_URL>(url.data());
    m_client->add<CURLOPT_USERAGENT>(SAFE_USER_AGENT.data()); // 403 without this
    m_client->add<CURLOPT_HTTPHEADER>(header.get());
    string answer = performPost();

    Value response;
    Json::Reader reader;

    if (!reader.parse(answer, response)) // invalid JSON (shouldn't happen)
        throw MailApiException("Error parsing shard response JSON");

    if (response["body"] != Value()) {
        return Shard(response["body"][Shard::as_string(type)]);
    }

    throw MailApiException("Non-Shard json received: " + answer);
}

void API::addUploadedFile(string name, string remote_dir, string hash_size)
{
    auto index = hash_size.find(';');
    auto end_index = hash_size.find('\r');
    if (index == string::npos)
        throw MailApiException("Non-hashsize answer received: " + hash_size);

    string file_hash = hash_size.substr(0, index);
    string file_size = hash_size.substr(index + 1, end_index - (index + 1));

    curl_header header;
    header.add("Accept: */*");
    header.add("Origin: " + CLOUD_DOMAIN);
    header.add("Referer: " + CLOUD_DOMAIN + "/home" + remote_dir);

    string post_fields = paramString({
        {"api", "2"},
        {"conflict", "rename"},
        {"home", remote_dir + name},
        {"hash", file_hash},
        {"size", file_size},
        {"token", m_token}
    });

    m_client->add<CURLOPT_URL>(SCLD_ADDFILE_ENDPOINT.data());
    m_client->add<CURLOPT_FOLLOWLOCATION>(1L);
    m_client->add<CURLOPT_VERBOSE>(1L);
    m_client->add<CURLOPT_USERAGENT>(SAFE_USER_AGENT.data()); // 403 without this
    m_client->add<CURLOPT_HTTPHEADER>(header.get());
    m_client->add<CURLOPT_POSTFIELDS>(post_fields.data());
    m_client->add<CURLOPT_DEBUGFUNCTION>(trace_post);
    performPost();
}


void API::upload(string path, string remote_dir)
{
    Shard s = obtainShard(Shard::ShardType::UPLOAD);

    string filename = path.substr(path.find_last_of("/\\") + 1);
    string upload_url = s.getUrl() + "?" + paramString({{"cloud_domain", "2"}, {"x-email", m_acc.login}});

    curl_header header;
    header.add("Accept: */*");
    header.add("Origin: " + CLOUD_DOMAIN);
    header.add("Referer: " + CLOUD_DOMAIN + "/home" + remote_dir);

    curl_form name_form;
    name_form.add(NV_FILEPAIR("file", path)); // fileupload part
    name_form.add(NV_PAIR("_file", filename)); // naming part

    curl_form file_form;

    m_client->add<CURLOPT_URL>(upload_url.data());
    m_client->add<CURLOPT_FOLLOWLOCATION>(1L);
    m_client->add<CURLOPT_USERAGENT>(SAFE_USER_AGENT.data()); // 403 without this
    m_client->add<CURLOPT_HTTPPOST>(name_form.get());
    m_client->add<CURLOPT_HTTPHEADER>(header.get());
    m_client->add<CURLOPT_VERBOSE>(1L);
    m_client->add<CURLOPT_DEBUGFUNCTION>(trace_post);
    string answer = performPost();

    addUploadedFile(filename, remote_dir, answer);

    return;
}

void API::mkdir(string remote_path)
{
    curl_header header;
    header.add("Accept: */*");
    header.add("Origin: " + CLOUD_DOMAIN);
    header.add("Referer: " + CLOUD_DOMAIN + "/home" + remote_path);

    string post_fields = paramString({
        {"api", "2"},
        {"conflict", "rename"},
        {"home", remote_path},
        {"token", m_token}
    });

    m_client->add<CURLOPT_URL>(SCLD_ADDFOLDER_ENDPOINT.data());
    m_client->add<CURLOPT_FOLLOWLOCATION>(1L);
    m_client->add<CURLOPT_USERAGENT>(SAFE_USER_AGENT.data()); // 403 without this
    m_client->add<CURLOPT_HTTPHEADER>(header.get());
    m_client->add<CURLOPT_POSTFIELDS>(post_fields.data());
    m_client->add<CURLOPT_VERBOSE>(1L);
    m_client->add<CURLOPT_DEBUGFUNCTION>(trace_post);
    performPost();
}
