assert_sum = None

def configure(config):
    global assert_sum
    assert_sum = Verifier(config["sum_total"])

class Verifier:
    __name__ = 'verifier'

    def __init__(self, sum_total):
       self._sum_total = sum_total

    def __call__(self, isum: int):
       assert isum == self._sum_total

