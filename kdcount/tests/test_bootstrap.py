import kdcount
import numpy

from kdcount import sphere, correlate
from kdcount import bootstrap
from numpy.testing import assert_allclose

def test_bootstrap():
    numpy.random.seed(1234)
    ra, dec = numpy.random.uniform(size=(2, 100000))
    ra = ra * 360
    dec = numpy.arcsin((dec - 0.5) * 2) / numpy.pi * 180
    Nbar = len(ra) / (4. * numpy.pi * (180 / numpy.pi) ** 2)

    ds = sphere.points(ra, dec)

    bsfun = lambda x: sphere.radec2pix(1, x.ra, x.dec)

    policy = bootstrap.policy(bsfun, ds)
    binning = sphere.AngularBinning(numpy.linspace(0, 1, 10))

    def func(ds):
        return len(ds)
    result = policy.run(func, ds)
    print(result.cache, result.sizes)
    for i in range(4):
        bs = policy.bootstrap()
        resample = policy.resample(result, bs)
        print(resample)

    def func2(ds1, ds2):
        return len(ds1) * len(ds2)
    result = policy.run(func2, ds, ds)

    print(result.cache, result.sizes)
    for i in range(1):
        bs = policy.bootstrap()
        resample = policy.resample(result, bs)
        print(resample)
