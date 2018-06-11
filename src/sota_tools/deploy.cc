#include "deploy.h"

#include <boost/filesystem.hpp>
#include <boost/intrusive_ptr.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>

#include "authenticate.h"
#include "logging/logging.h"
#include "ostree_dir_repo.h"
#include "ostree_http_repo.h"
#include "ostree_object.h"
#include "ostree_ref.h"
#include "rate_controller.h"
#include "request_pool.h"
#include "treehub_server.h"

bool UploadToTreehub(const OSTreeRepo::ptr &src_repo, const ServerCredentials &push_credentials,
                     const OSTreeHash &ostree_commit, const std::string &cacerts, const bool dryrun,
                     const int max_curl_requests) {
  TreehubServer push_server;
  assert(max_curl_requests > 0);
  if (authenticate(cacerts, push_credentials, push_server) != EXIT_SUCCESS) {
    LOG_FATAL << "Authentication failed";
    return false;
  }

  OSTreeObject::ptr root_object;
  try {
    root_object = src_repo->GetObject(ostree_commit);
  } catch (const OSTreeObjectMissing &error) {
    LOG_FATAL << "OSTree Commit " << ostree_commit << " was not found in src repository";
    return false;
  }

  RequestPool request_pool(push_server, max_curl_requests);

  // Add commit object to the queue
  request_pool.AddQuery(root_object);

  // Main curl event loop.
  // request_pool takes care of holding number of outstanding requests below
  // OSTreeObject::CurlDone() adds new requests to the pool and
  // stop the pool on error

  do {
    request_pool.Loop(dryrun);
  } while (root_object->is_on_server() != PresenceOnServer::kObjectPresent && !request_pool.is_stopped());

  if (root_object->is_on_server() == PresenceOnServer::kObjectPresent) {
    if (!dryrun) {
      LOG_INFO << "Upload to Treehub complete after " << request_pool.total_requests_made() << " requests";
    } else {
      LOG_INFO << "Dry run. No objects uploaded.";
    }
  } else {
    LOG_ERROR << "One or more errors while pushing";
  }

  return root_object->is_on_server() == PresenceOnServer::kObjectPresent;
}

bool OfflineSignRepo(const ServerCredentials &push_credentials, const std::string &name, const OSTreeHash &hash,
                     const std::string &hardwareids) {
  if (!boost::filesystem::is_directory(boost::filesystem::path("./tuf/aktualizr"))) {
    std::string init_cmd("garage-sign init --repo aktualizr --credentials ");
    if (system((init_cmd + push_credentials.GetPathOnDisk().string()).c_str()) != 0) {
      LOG_ERROR << "Could not initilaize tuf repo for sign";
      return false;
    }
  }

  if (system("garage-sign targets  pull --repo aktualizr") != 0) {
    LOG_ERROR << "Could not pull targets";
    return false;
  }

  std::string cmd("garage-sign targets add --repo aktualizr --format OSTREE --length 0 --url \"https://example.com/\"");
  cmd += " --name " + name + " --version " + hash.string() + " --sha256 " + hash.string();
  cmd += " --hardwareids " + hardwareids;
  if (system(cmd.c_str()) != 0) {
    LOG_ERROR << "Could not add targets";
    return false;
  }

  LOG_INFO << "Signing...\n";
  if (system("garage-sign targets  sign --key-name targets --repo aktualizr") != 0) {
    LOG_ERROR << "Could not sign targets";
    return false;
  }
  if (system("garage-sign targets  push --repo aktualizr") != 0) {
    LOG_ERROR << "Could not push signed repo";
    return false;
  }

  LOG_INFO << "Success";
  return true;
}

bool PushRootRef(const ServerCredentials &push_credentials, const OSTreeRef &ref, const std::string &cacerts,
                 bool dry_run) {
  if (push_credentials.CanSignOffline()) {
    // In general, this is the wrong thing.  We should be using offline signing
    // if private key material is present in credentials.zip
    LOG_WARNING << "Pushing by refname when credentials.zip can be used to sign-offline.";
  }

  TreehubServer push_server;

  if (authenticate(cacerts, push_credentials, push_server) != EXIT_SUCCESS) {
    LOG_FATAL << "Authentication failed";
    return false;
  }

  bool ok = true;
  if (!dry_run) {
    CURL *easy_handle = curl_easy_init();
    curl_easy_setopt(easy_handle, CURLOPT_VERBOSE, get_curlopt_verbose());
    ref.PushRef(push_server, easy_handle);
    CURLcode err = curl_easy_perform(easy_handle);
    if (err != 0u) {
      LOG_ERROR << "Error pushing root ref:" << curl_easy_strerror(err);
      ok = false;
    }
    long rescode;  // NOLINT
    curl_easy_getinfo(easy_handle, CURLINFO_RESPONSE_CODE, &rescode);
    if (rescode != 200) {
      LOG_ERROR << "Error pushing root ref, got " << rescode << " HTTP response";
      ok = false;
    }
    curl_easy_cleanup(easy_handle);
  }
  return ok;
}
