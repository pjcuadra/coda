/* BLURB gpl
                           Coda File System
                              Release 7
          Copyright (c) 1987-2019 Carnegie Mellon University
                  Additional copyrights listed below
This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.
                        Additional copyrights
                           none currently
#*/
/* system */
#include <sys/stat.h>

/* external */
#include <gtest/gtest.h>

/* from coda-src */
#include <venus/conf.h>
#include <venus/realm.h>
#include <venus/user/user.h>
#include <venus/user/userfactory.h>

using namespace std;

namespace
{
class UserEntTest : public ::testing::Test {
protected:
    void SetUp() override {}

    void TearDown() override {}
};

static bool connected = false;

class TestUserEnt : public userent {
public:
    TestUserEnt(RealmId realmid, uid_t userid)
        : userent(realmid, userid)
    {
    }
    long GetTokens(SecretToken *secret, ClearToken *token)
    {
        memset(secret, 1, sizeof(SecretToken));
        memset(token, 1, sizeof(ClearToken));
        return 0;
    }
    int TokensValid() { return 1; }
    void Invalidate() {}
    void Reset() {}
    int Connect(RPC2_Handle *, int *, struct in_addr *)
    {
        connected = true;
        return 0;
    }
    void SetWaitForever(int) {}
    void CheckTokenExpiry() {}
    userent *instanceOf(RealmId realmid, uid_t userid)
    {
        return new TestUserEnt(realmid, userid);
    }
};

TEST_F(UserEntTest, dummyConnect)
{
    userent *user = NULL;
    UserFactory::setPrototype(new TestUserEnt(0, 0));
    user = UserFactory::create(0, 7);
    ASSERT_TRUE(user);
    ASSERT_EQ(user->GetUid(), 7);

    connected = false;

    user->Connect(NULL, NULL, NULL);
    ASSERT_TRUE(connected);
}

TEST_F(UserEntTest, realmGetUser)
{
    Realm realm   = Realm(8, "myrealm.test.edu");
    userent *user = NULL;
    UserFactory::setPrototype(new TestUserEnt(0, 0));
    ASSERT_TRUE(false);
    user = realm.GetUser(5);
    ASSERT_TRUE(false);
    ASSERT_TRUE(user);
    ASSERT_EQ(user->GetUid(), 7);

    connected = false;

    user->Connect(NULL, NULL, NULL);
    ASSERT_TRUE(connected);
}

} // namespace
