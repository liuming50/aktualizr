#include <gtest/gtest.h>

#include <iostream>
#include <string>

#include <boost/filesystem.hpp>

#include "config/config.h"
#include "logging/logging.h"
#include "test_utils.h"
#include "uptane_repo.h"

KeyType key_type = KeyType::kED25519;
std::string generate_repo_exec;

void check_repo(boost::filesystem::path repo_dir) {
  Json::Value targets = Utils::parseJSONFile(repo_dir / "targets.json")["signed"];
  std::string signed_targets = Utils::readFile(repo_dir / "targets.json");

  Json::Value snapshot = Utils::parseJSONFile(repo_dir / "snapshot.json")["signed"];
  EXPECT_EQ(snapshot["meta"]["targets.json"]["hashes"]["sha256"].asString(),
            boost::algorithm::to_lower_copy(boost::algorithm::hex(Crypto::sha256digest(signed_targets))));
  EXPECT_EQ(snapshot["meta"]["targets.json"]["hashes"]["sha512"].asString(),
            boost::algorithm::to_lower_copy(boost::algorithm::hex(Crypto::sha512digest(signed_targets))));
  EXPECT_EQ(snapshot["meta"]["targets.json"]["length"].asUInt(), static_cast<Json::UInt>(signed_targets.length()));
  EXPECT_EQ(snapshot["meta"]["targets.json"]["version"].asUInt(), targets["version"].asUInt());

  auto signed_snapshot = Utils::readFile(repo_dir / "snapshot.json");
  Json::Value timestamp = Utils::parseJSONFile(repo_dir / "timestamp.json")["signed"];
  EXPECT_EQ(timestamp["meta"]["snapshot.json"]["hashes"]["sha256"].asString(),
            boost::algorithm::to_lower_copy(boost::algorithm::hex(Crypto::sha256digest(signed_snapshot))));
  EXPECT_EQ(timestamp["meta"]["snapshot.json"]["hashes"]["sha512"].asString(),
            boost::algorithm::to_lower_copy(boost::algorithm::hex(Crypto::sha512digest(signed_snapshot))));
  EXPECT_EQ(timestamp["meta"]["snapshot.json"]["length"].asUInt(), static_cast<Json::UInt>(signed_snapshot.length()));
  EXPECT_EQ(timestamp["meta"]["snapshot.json"]["version"].asUInt(), snapshot["version"].asUInt());
}

void check_repo(const TemporaryDirectory &temp_dir) {
  check_repo(temp_dir.Path() / "repo/image");
  check_repo(temp_dir.Path() / "repo/director");
}
/*
 * Generate images and director repos.
 */
TEST(aktualizr_repo, generate_repo) {
  TemporaryDirectory temp_dir;
  UptaneRepo repo(temp_dir.Path(), "", "correlation");
  repo.generateRepo(key_type);
  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / "repo/director/root.json"));
  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / "repo/director/1.root.json"));
  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / "repo/director/targets.json"));
  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / "repo/director/timestamp.json"));
  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / "repo/image/root.json"));
  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / "repo/image/1.root.json"));
  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / "repo/image/targets.json"));
  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / "repo/image/timestamp.json"));
  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / "repo/director/manifest"));

  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / "keys/image/root/private.key"));
  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / "keys/image/root/public.key"));
  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / "keys/image/snapshot/private.key"));
  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / "keys/image/snapshot/public.key"));
  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / "keys/image/targets/private.key"));
  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / "keys/image/targets/public.key"));
  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / "keys/image/timestamp/private.key"));
  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / "keys/image/timestamp/public.key"));

  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / "keys/director/root/private.key"));
  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / "keys/director/root/public.key"));
  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / "keys/director/snapshot/private.key"));
  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / "keys/director/snapshot/public.key"));
  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / "keys/director/targets/private.key"));
  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / "keys/director/targets/public.key"));
  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / "keys/director/timestamp/private.key"));
  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / "keys/director/timestamp/public.key"));

  std::vector<std::string> keys;
  std::function<void(const boost::filesystem::path &path)> recursive_check;
  recursive_check = [&keys, &recursive_check](const boost::filesystem::path &path) {
    for (auto &p : boost::filesystem::directory_iterator(path)) {
      if (p.status().type() == boost::filesystem::file_type::directory_file) {
        recursive_check(p.path());
      } else {
        if (p.path().filename().string() == "key_type") {
          continue;
        }
        std::string hash = Crypto::sha512digest(Utils::readFile(p.path()));
        if (std::find(keys.begin(), keys.end(), hash) == keys.end()) {
          keys.push_back(hash);
        } else {
          FAIL() << p.path().string() << " is not unique";
        }
      }
    }
  };
  recursive_check(temp_dir.Path() / "keys");

  Json::Value image_targets = Utils::parseJSONFile(temp_dir.Path() / "repo/image/targets.json");
  EXPECT_EQ(image_targets["signed"]["targets"].size(), 0);
  Json::Value director_targets = Utils::parseJSONFile(temp_dir.Path() / "repo/director/targets.json");
  EXPECT_EQ(director_targets["signed"]["targets"].size(), 0);
  EXPECT_EQ(director_targets["signed"]["custom"]["correlationId"], "correlation");
  check_repo(temp_dir);
}

/*
 * Add an image to the images repo.
 */
TEST(aktualizr_repo, add_image) {
  TemporaryDirectory temp_dir;
  UptaneRepo repo(temp_dir.Path(), "", "");
  repo.generateRepo(key_type);
  repo.addImage(temp_dir.Path() / "repo/director/manifest", "repo/director/manifest", {});
  Json::Value image_targets = Utils::parseJSONFile(temp_dir.Path() / "repo/image/targets.json");
  EXPECT_EQ(image_targets["signed"]["targets"].size(), 1);
  Json::Value director_targets = Utils::parseJSONFile(temp_dir.Path() / "repo/director/targets.json");
  EXPECT_EQ(director_targets["signed"]["targets"].size(), 0);
  check_repo(temp_dir);
}

/*
 * Copy an image to the director repo.
 */
TEST(aktualizr_repo, copy_image) {
  TemporaryDirectory temp_dir;
  UptaneRepo repo(temp_dir.Path(), "", "");
  repo.generateRepo(key_type);
  repo.addImage(temp_dir.Path() / "repo/director/manifest", "manifest", {});
  repo.addTarget("manifest", "test-hw", "test-serial");
  repo.signTargets();
  Json::Value image_targets = Utils::parseJSONFile(temp_dir.Path() / "repo/image/targets.json");
  EXPECT_EQ(image_targets["signed"]["targets"].size(), 1);
  Json::Value director_targets = Utils::parseJSONFile(temp_dir.Path() / "repo/director/targets.json");
  EXPECT_EQ(director_targets["signed"]["targets"].size(), 1);
  check_repo(temp_dir);
}

/*
 * Add simple delegation.
 * Add image with delegation.
 */
TEST(aktualizr_repo, delegation) {
  TemporaryDirectory temp_dir;
  std::ostringstream keytype_stream;
  keytype_stream << key_type;
  std::string cmd = generate_repo_exec + " generate " + temp_dir.Path().string() + " --keytype " + keytype_stream.str();
  std::string output;
  int retval = Utils::shell(cmd, &output);
  if (retval) {
    FAIL() << "'" << cmd << "' exited with error code " << retval << "\n";
  }
  cmd = generate_repo_exec + " adddelegation " + temp_dir.Path().string() + " --keytype " + keytype_stream.str();
  cmd += " --dname test_delegate --dpattern 'tests/test_data/*.txt' ";
  retval = Utils::shell(cmd, &output);
  if (retval) {
    FAIL() << "'" << cmd << "' exited with error code " << retval << "\n";
  }

  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / "repo/image/delegations/test_delegate.json"));
  auto targets = Utils::parseJSONFile(temp_dir.Path() / "repo/image/targets.json");
  EXPECT_EQ(targets["signed"]["delegations"]["roles"][0]["name"].asString(), "test_delegate");
  EXPECT_EQ(targets["signed"]["delegations"]["roles"][0]["paths"][0].asString(), "tests/test_data/*.txt");

  cmd = generate_repo_exec + " image " + temp_dir.Path().string() + " --keytype " + keytype_stream.str();
  cmd += " --dname test_delegate --filename tests/test_data/firmware.txt";
  retval = Utils::shell(cmd, &output);
  if (retval) {
    FAIL() << "'" << output << "' exited with error code " << retval << "\n";
  }
  {
    auto test_delegate = Utils::parseJSONFile(temp_dir.Path() / "repo/image/delegations/test_delegate.json");
    Uptane::Targets delegate_targets(test_delegate);
    EXPECT_EQ(delegate_targets.targets.size(), 1);
    EXPECT_EQ(delegate_targets.targets[0].filename(), "tests/test_data/firmware.txt");
    EXPECT_EQ(delegate_targets.targets[0].length(), 17);
    EXPECT_EQ(delegate_targets.targets[0].sha256Hash(),
              "d8e9caba8c1697fcbade1057f9c2488044192ff76bb64d4aba2c20e53dc33033");
  }
  cmd = generate_repo_exec + " image " + temp_dir.Path().string() + " --keytype " + keytype_stream.str();
  cmd +=
      " --dname test_delegate --targetname tests/test_data/firmware2.txt --targetsha256 "
      "d8e9caba8c1697fcbade1057f9c2488044192ff76bb64d4aba2c20e53dc33033 --targetlength 17";
  retval = Utils::shell(cmd, &output);
  if (retval) {
    FAIL() << "'" << cmd << "' exited with error code " << retval << "\n";
  }
  {
    auto test_delegate = Utils::parseJSONFile(temp_dir.Path() / "repo/image/delegations/test_delegate.json");
    Uptane::Targets delegate_targets(test_delegate);
    EXPECT_EQ(delegate_targets.targets.size(), 2);
    EXPECT_EQ(delegate_targets.targets[1].filename(), "tests/test_data/firmware2.txt");
    EXPECT_EQ(delegate_targets.targets[1].length(), 17);
    EXPECT_EQ(delegate_targets.targets[1].sha256Hash(),
              "d8e9caba8c1697fcbade1057f9c2488044192ff76bb64d4aba2c20e53dc33033");
  }
  check_repo(temp_dir);
}

TEST(aktualizr_repo, delegation_revoke) {
  TemporaryDirectory temp_dir;
  std::ostringstream keytype_stream;
  keytype_stream << key_type;
  std::string cmd = generate_repo_exec + " generate " + temp_dir.Path().string() + " --keytype " + keytype_stream.str();
  std::string output;
  int retval = Utils::shell(cmd, &output);
  if (retval) {
    FAIL() << "'" << cmd << "' exited with error code " << retval << "\n";
  }
  cmd = generate_repo_exec + " adddelegation " + temp_dir.Path().string() + " --keytype " + keytype_stream.str();
  cmd += " --dname test_delegate --dpattern 'tests/test_data/*.txt' ";
  retval = Utils::shell(cmd, &output);
  if (retval) {
    FAIL() << "'" << cmd << "' exited with error code " << retval << "\n";
  }

  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / "repo/image/delegations/test_delegate.json"));
  auto targets = Utils::parseJSONFile(temp_dir.Path() / "repo/image/targets.json");
  EXPECT_EQ(targets["signed"]["delegations"]["roles"][0]["name"].asString(), "test_delegate");
  EXPECT_EQ(targets["signed"]["delegations"]["roles"][0]["paths"][0].asString(), "tests/test_data/*.txt");

  cmd = generate_repo_exec + " image " + temp_dir.Path().string() + " --keytype " + keytype_stream.str();
  cmd += " --dname test_delegate --filename tests/test_data/firmware.txt";
  retval = Utils::shell(cmd, &output);
  if (retval) {
    FAIL() << "'" << output << "' exited with error code " << retval << "\n";
  }
  {
    auto test_delegate = Utils::parseJSONFile(temp_dir.Path() / "repo/image/delegations/test_delegate.json");
    Uptane::Targets delegate_targets(test_delegate);
    EXPECT_EQ(delegate_targets.targets.size(), 1);
    EXPECT_EQ(delegate_targets.targets[0].filename(), "tests/test_data/firmware.txt");
    EXPECT_EQ(delegate_targets.targets[0].length(), 17);
    EXPECT_EQ(delegate_targets.targets[0].sha256Hash(),
              "d8e9caba8c1697fcbade1057f9c2488044192ff76bb64d4aba2c20e53dc33033");
  }
  cmd = generate_repo_exec + " image " + temp_dir.Path().string() + " --keytype " + keytype_stream.str();
  cmd +=
      " --dname test_delegate --targetname tests/test_data/firmware2.txt --targetsha256 "
      "d8e9caba8c1697fcbade1057f9c2488044192ff76bb64d4aba2c20e53dc33033 --targetlength 17";
  retval = Utils::shell(cmd, &output);
  if (retval) {
    FAIL() << "'" << cmd << "' exited with error code " << retval << "\n";
  }

  cmd = generate_repo_exec + " addtarget " + temp_dir.Path().string() + " --keytype " + keytype_stream.str();
  cmd += " --hwid primary_hw --serial CA:FE:A6:D2:84:9D --targetname tests/test_data/firmware.txt";
  retval = Utils::shell(cmd, &output);
  if (retval) {
    FAIL() << "'" << cmd << "' exited with error code " << retval << "\n";
  }

  cmd = generate_repo_exec + " signtargets " + temp_dir.Path().string() + " --keytype " + keytype_stream.str();
  retval = Utils::shell(cmd, &output);
  if (retval) {
    FAIL() << "'" << cmd << "' exited with error code " << retval << "\n";
  }
  {
    auto test_delegate = Utils::parseJSONFile(temp_dir.Path() / "repo/image/delegations/test_delegate.json");
    Uptane::Targets delegate_targets(test_delegate);
    EXPECT_EQ(delegate_targets.targets.size(), 2);
    EXPECT_EQ(delegate_targets.targets[1].filename(), "tests/test_data/firmware2.txt");
    EXPECT_EQ(delegate_targets.targets[1].length(), 17);
    EXPECT_EQ(delegate_targets.targets[1].sha256Hash(),
              "d8e9caba8c1697fcbade1057f9c2488044192ff76bb64d4aba2c20e53dc33033");
  }
  {
    auto signed_targets = Utils::parseJSONFile(temp_dir.Path() / "repo/director/targets.json");
    Uptane::Targets director_targets(signed_targets);
    EXPECT_EQ(director_targets.targets.size(), 1);
    EXPECT_EQ(director_targets.targets[0].filename(), "tests/test_data/firmware.txt");
    EXPECT_EQ(director_targets.targets[0].length(), 17);
    EXPECT_EQ(director_targets.targets[0].sha256Hash(),
              "d8e9caba8c1697fcbade1057f9c2488044192ff76bb64d4aba2c20e53dc33033");
  }
  check_repo(temp_dir);

  cmd = generate_repo_exec + " revokedelegation " + temp_dir.Path().string() + " --keytype " + keytype_stream.str();
  cmd += " --dname test_delegate";
  retval = Utils::shell(cmd, &output);
  if (retval) {
    FAIL() << "'" << cmd << "' exited with error code " << retval << "\n";
  }
  EXPECT_FALSE(boost::filesystem::exists(temp_dir.Path() / "repo/image/delegations/test_delegate.json"));
  auto new_targets = Utils::parseJSONFile(temp_dir.Path() / "repo/image/targets.json");
  EXPECT_EQ(new_targets["signed"]["delegations"]["keys"].size(), 0);
  EXPECT_EQ(new_targets["signed"]["delegations"]["roles"].size(), 0);
  EXPECT_EQ(new_targets["signed"]["version"].asUInt(), 3);
  auto signed_targets = Utils::parseJSONFile(temp_dir.Path() / "repo/director/targets.json");
  Uptane::Targets director_targets(signed_targets);
  EXPECT_EQ(director_targets.targets.size(), 0);
}

/*
 * Sign arbitrary metadata.
 */
TEST(aktualizr_repo, sign) {
  TemporaryDirectory temp_dir;
  std::ostringstream keytype_stream;
  keytype_stream << key_type;
  std::string cmd = generate_repo_exec + " generate " + temp_dir.Path().string() + " --keytype " + keytype_stream.str();
  std::string output;
  int retval = Utils::shell(cmd, &output);
  if (retval) {
    FAIL() << "'" << cmd << "' exited with error code " << retval << "\n";
  }
  cmd = generate_repo_exec + " sign " + temp_dir.Path().string();
  cmd += " --repotype director --keyname snapshot";
  std::string sign_cmd =
      "echo \"{\\\"_type\\\":\\\"Snapshot\\\",\\\"expires\\\":\\\"2021-07-04T16:33:27Z\\\"}\" | " + cmd;
  output.clear();
  retval = Utils::shell(sign_cmd, &output);
  if (retval) {
    FAIL() << "'" << sign_cmd << "' exited with error code " << retval << "\n";
  }
  auto json = Utils::parseJSON(output);
  Uptane::Root root(Uptane::RepositoryType::Director(),
                    Utils::parseJSONFile(temp_dir.Path() / "repo/director/root.json"));
  root.UnpackSignedObject(Uptane::RepositoryType::Director(), Uptane::Role::Snapshot(), json);
  EXPECT_NO_THROW(root.UnpackSignedObject(Uptane::RepositoryType::Director(), Uptane::Role::Snapshot(), json));
  check_repo(temp_dir);
}

/*
 * Add custom image metadata without an actual file.
 */
TEST(aktualizr_repo, image_custom) {
  TemporaryDirectory temp_dir;
  std::ostringstream keytype_stream;
  keytype_stream << key_type;
  std::string cmd = generate_repo_exec + " generate " + temp_dir.Path().string() + " --keytype " + keytype_stream.str();
  std::string output;
  int retval = Utils::shell(cmd, &output);
  if (retval) {
    FAIL() << "'" << cmd << "' exited with error code " << retval << "\n";
  }
  cmd = generate_repo_exec + " image " + temp_dir.Path().string();
  cmd +=
      " --targetname target1 --targetsha256 8ab755c16de6ee9b6224169b36cbf0f2a545f859be385501ad82cdccc240d0a6 "
      "--targetlength 123";
  retval = Utils::shell(cmd, &output);
  if (retval) {
    FAIL() << "'" << cmd << "' exited with error code " << retval << "\n";
  }

  Json::Value image_targets = Utils::parseJSONFile(temp_dir.Path() / "repo/image/targets.json");
  EXPECT_EQ(image_targets["signed"]["targets"].size(), 1);
  EXPECT_EQ(image_targets["signed"]["targets"]["target1"]["length"].asUInt(), 123);
  check_repo(temp_dir);
}

/*
 * Clear the staged director targets metadata.
 */
TEST(aktualizr_repo, emptytargets) {
  TemporaryDirectory temp_dir;
  std::ostringstream keytype_stream;
  keytype_stream << key_type;
  std::string cmd = generate_repo_exec + " generate " + temp_dir.Path().string() + " --keytype " + keytype_stream.str();
  std::string output;
  int retval = Utils::shell(cmd, &output);
  if (retval) {
    FAIL() << "'" << cmd << "' exited with error code " << retval << "\n";
  }
  cmd = generate_repo_exec + " image " + temp_dir.Path().string();
  cmd +=
      " --targetname target1 --targetsha256 8ab755c16de6ee9b6224169b36cbf0f2a545f859be385501ad82cdccc240d0a6 "
      "--targetlength 123";
  retval = Utils::shell(cmd, &output);
  if (retval) {
    FAIL() << "'" << cmd << "' exited with error code " << retval << "\n";
  }

  cmd = generate_repo_exec + " addtarget " + temp_dir.Path().string();
  cmd += " --targetname target1 --hwid hwid123 --serial serial123";
  retval = Utils::shell(cmd, &output);
  if (retval) {
    FAIL() << "'" << cmd << "' exited with error code " << retval << "\n";
  }

  Json::Value targets = Utils::parseJSONFile(temp_dir.Path() / "repo/director/staging/targets.json");
  EXPECT_EQ(targets["targets"].size(), 1);
  EXPECT_EQ(targets["targets"]["target1"]["length"].asUInt(), 123);
  EXPECT_EQ(targets["targets"]["target1"]["hashes"]["sha256"].asString(),
            "8AB755C16DE6EE9B6224169B36CBF0F2A545F859BE385501AD82CDCCC240D0A6");

  cmd = generate_repo_exec + " emptytargets " + temp_dir.Path().string();
  retval = Utils::shell(cmd, &output);
  if (retval) {
    FAIL() << "'" << cmd << "' exited with error code " << retval << "\n"
           << "output: " << output;
  }

  Json::Value empty_targets = Utils::parseJSONFile(temp_dir.Path() / "repo/director/staging/targets.json");
  EXPECT_EQ(empty_targets["targets"].size(), 0);
  check_repo(temp_dir);
}

/*
 * Populate the director targets metadata with the currently signed metadata.
 */
TEST(aktualizr_repo, oldtargets) {
  TemporaryDirectory temp_dir;
  UptaneRepo repo(temp_dir.Path(), "", "");
  repo.generateRepo(key_type);
  Uptane::Hash hash(Uptane::Hash::Type::kSha256, "8ab755c16de6ee9b6224169b36cbf0f2a545f859be385501ad82cdccc240d0a6");
  repo.addCustomImage("target1", hash, 123);
  repo.addCustomImage("target2", hash, 321);
  repo.addTarget("target1", "test-hw", "test-serial");
  repo.signTargets();
  repo.addTarget("target2", "test-hw", "test-serial");

  Json::Value targets = Utils::parseJSONFile(temp_dir.Path() / "repo/director/staging/targets.json");
  EXPECT_EQ(targets["targets"].size(), 2);
  EXPECT_EQ(targets["targets"]["target1"]["length"].asUInt(), 123);
  EXPECT_EQ(targets["targets"]["target1"]["hashes"]["sha256"].asString(),
            "8AB755C16DE6EE9B6224169B36CBF0F2A545F859BE385501AD82CDCCC240D0A6");
  EXPECT_EQ(targets["targets"]["target2"]["length"].asUInt(), 321);
  EXPECT_EQ(targets["targets"]["target2"]["hashes"]["sha256"].asString(),
            "8AB755C16DE6EE9B6224169B36CBF0F2A545F859BE385501AD82CDCCC240D0A6");

  Json::Value targets_current = Utils::parseJSONFile(temp_dir.Path() / "repo/director/targets.json");
  EXPECT_EQ(targets_current["signed"]["targets"].size(), 1);
  EXPECT_EQ(targets_current["signed"]["targets"]["target1"]["length"].asUInt(), 123);
  EXPECT_EQ(targets_current["signed"]["targets"]["target1"]["hashes"]["sha256"].asString(),
            "8AB755C16DE6EE9B6224169B36CBF0F2A545F859BE385501AD82CDCCC240D0A6");

  std::string cmd = generate_repo_exec + " oldtargets " + temp_dir.Path().string();
  std::string output;
  int retval = Utils::shell(cmd, &output);
  if (retval) {
    FAIL() << "'" << cmd << "' exited with error code " << retval << "\n"
           << "output: " << output;
  }
  targets = Utils::parseJSONFile(temp_dir.Path() / "repo/director/staging/targets.json");
  EXPECT_EQ(targets["targets"].size(), 1);
  EXPECT_EQ(targets["targets"]["target1"]["length"].asUInt(), 123);
  EXPECT_EQ(targets["targets"]["target1"]["hashes"]["sha256"].asString(),
            "8AB755C16DE6EE9B6224169B36CBF0F2A545F859BE385501AD82CDCCC240D0A6");
  check_repo(temp_dir);
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  logger_init();
  logger_set_threshold(boost::log::trivial::trace);
  if (argc >= 2) {
    generate_repo_exec = argv[1];
  } else {
    std::cerr << "No generate-repo executable specified\n";
    return EXIT_FAILURE;
  }

  // note: we used to run tests for all key types, which was pretty slow.
  // Now, they only run with ed25519
  return RUN_ALL_TESTS();
}
#endif
