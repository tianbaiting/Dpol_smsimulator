"""Make `from geom_acceptance import ...` work when pytest collects from repo root.

Without this, the presence of __init__.py makes pytest import these test files
as the package 'nebula_reco_quality.test_geom_acceptance', which can't resolve
the bare 'geom_acceptance' name. Inserting the package dir into sys.path lets
bare imports work regardless of how pytest is invoked.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
