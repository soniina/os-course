from base_test import BaseShellTest


class TestShellRedirection(BaseShellTest):
    def test_invalid_redirection_syntax(self):
        self.execute("echo test foo bar>bbb", "test foo bar>bbb")
        self.execute("echo test<aaa>bbb", "test<aaa>bbb")
