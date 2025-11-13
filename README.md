# negentropic-core

[![License: MIT OR GPL-3.0](https://img.shields.io/badge/License-MIT%20OR%20GPL--3.0-blue.svg)](LICENSE)
[![Version](https://img.shields.io/badge/version-v0.1.0--alpha-orange)](https://github.com/your-org/negentropic-core/releases)
[![Python](https://img.shields.io/badge/python-3.9+-blue.svg)](https://www.python.org/downloads/)
[![TypeScript](https://img.shields.io/badge/typescript-5.3+-blue.svg)](https://www.typescriptlang.org/)

> **negentropic-core** is the physics and systems engine powering the Negentropic ecosystem.
> Built from the open-science-dlt computational substrate, it provides SE(3)-based physical solvers for atmosphere, hydrology, and soil processes.
> It serves both scientific simulation (Python API) and real-time applications (Unity bindings).

---

## 🌟 Key Features

*   **SE(3)-Based Dynamics**: Core solvers operate on Lie groups for geometrically consistent and accurate physical modeling
*   **Modular Physics Solvers**: Independently testable modules for atmospheric, hydrological, and soil systems
*   **Multi-Scale Architecture**: From edge devices (ESP32-S3) to cloud-scale scientific computing
*   **Dual-Use Design**: Python API for scientific workflows + TypeScript/Unity bindings for real-time applications
*   **Embedded Computing**: Fixed-point C implementation for resource-constrained edge devices
*   **Open Science Provenance**: Full mathematical attribution to UCF and peer-reviewed sources

---

## 📦 Quick Start

### Prerequisites

*   **Python 3.9+** with pip
*   **Node.js 18+** with npm (for TypeScript components)
*   **GCC/Clang** (for embedded C components, optional)

### Installation

```bash
# Clone the repository
git clone https://github.com/your-org/negentropic-core.git
cd negentropic-core

# Install Python dependencies
pip install -r requirements.txt

# Install TypeScript dependencies (optional)
npm install

# Verify installation with examples
python examples/atmosphere_demo.py
```

### Python Quick Example

```python
from simulation import SimulationKernel, SimulationConfig
from physics import AtmosphereSolver

# Configure simulation
config = SimulationConfig(
    grid_size=(32, 32, 16),
    dt=60.0,
    max_steps=100,
    enable_atmosphere=True
)

# Run simulation
kernel = SimulationKernel(config)
kernel.initialize({})
results = kernel.run()

print(f"Final diagnostics: {results['diagnostics']}")
```

### TypeScript Quick Example

```typescript
import { scienceClient } from './src/core/client';

// Compute SE(3) regenerative metrics
const metrics = await scienceClient.computeMetrics({
  poses: [
    { rotation: [0.1, 0, 0], translation: [0.5, 0, 0] },
    { rotation: [0, 0.1, 0], translation: [0, 0.5, 0] }
  ]
});

console.log(`Optimal λ: ${metrics.optimal_lambda}`);
```

---

## 🏗️ Repository Structure

```
negentropic-core/
├── src/
│   ├── core/                   # SE(3) Lie group mathematics (from UCF)
│   ├── physics/                # Atmospheric, hydrological, soil solvers
│   ├── simulation/             # Multi-physics kernel & time integration
│   ├── io/                     # Data adapters (planned)
│   └── utils/                  # Utilities (planned)
├── embedded/                   # ESP32-S3 fixed-point implementation
├── examples/                   # Demonstration scripts
├── tests/                      # Unit tests
├── tools/                      # Build utilities
├── docs/                       # Documentation
└── config/                     # Configuration templates (planned)
```

See [docs/core-architecture.md](docs/core-architecture.md) for detailed architecture.

---

## 🎯 Current Status (v0.1.0-alpha)

### ✅ Complete
- [x] SE(3) core mathematics (ported from UCF)
- [x] Flask REST API + TypeScript client
- [x] Embedded fixed-point math core (ESP32-S3)
- [x] Project structure and configuration
- [x] Dual MIT + GPL licensing
- [x] Example demonstrations

### ⚠️ In Progress (Stub Implementations)
- [ ] Physics solvers (atmosphere, hydrology, soil)
- [ ] Simulation kernel coupling
- [ ] SE(3) time integrators
- [ ] Adaptive time stepping

### 📋 Planned
- [ ] Full SE(3)-based physics implementations
- [ ] Data I/O adapters (NASADEM, HLS, BOM)
- [ ] Unity C# bindings
- [ ] Comprehensive test suite
- [ ] Performance profiling

---

## 🚀 Use Cases

### Scientific Simulation
- Atmospheric modeling with geometric consistency
- Groundwater flow and contaminant transport
- Soil moisture dynamics and carbon cycling
- Multi-scale coupled Earth system models

### Real-Time Applications
- Negentropic game environment simulation
- Digital twin sensor networks
- Agricultural monitoring and optimization
- Climate adaptation planning tools

### Edge Computing
- Marine AIS trajectory analysis (ESP32-S3)
- Distributed environmental sensing
- Low-power λ-estimation for regenerative metrics

---

## 📚 Examples

### Run All Demos

```bash
# SE(3) regenerative metrics
python examples/example_usage.py

# Physics demonstrations
python examples/atmosphere_demo.py
python examples/groundwater_demo.py
python examples/soil_moisture_demo.py
```

### Available Examples

| Example | Description | Status |
|---------|-------------|--------|
| `example_usage.py` | SE(3) metrics service demonstrations | ✅ Complete |
| `atmosphere_demo.py` | Atmospheric diffusion with temperature hotspot | ⚠️ Stub |
| `groundwater_demo.py` | Groundwater flow and water table dynamics | ⚠️ Stub |
| `soil_moisture_demo.py` | Soil moisture infiltration and carbon cycling | ⚠️ Stub |

---

## 🧪 Testing

### Python Unit Tests
```bash
cd src/core/tests
pytest test_metrics_service.py -v
```

### Embedded C Tests
```bash
cd tests
make test
```

Expected output:
```
======================================================================
SE(3) FIXED-POINT MATHEMATICS - UNIT TEST SUITE
======================================================================
...
Passed: 39
Failed: 0
✓ ALL TESTS PASSED
```

---

## 📖 Documentation

- **[Architecture Guide](docs/core-architecture.md)** - Detailed system architecture
- **[API Reference](#)** - Coming soon
- **[Integration Guide](#)** - Coming soon
- **[Mathematical Foundation](https://github.com/dj-ccs/Unified-Conscious-Evolution-Framework)** - UCF repository

---

## 🔗 Provenance & Attribution

### Mathematical Foundation
- **Source:** [Unified Conscious Evolution Framework (UCF)](https://github.com/dj-ccs/Unified-Conscious-Evolution-Framework)
- **License:** MIT
- **Reference:** Eckmann & Tlusty (2025), arXiv:2502.14367
- **Mathematical Core:** SE(3) Double-and-Scale Regenerative Return

### Codebase Origin
- **Ported From:** [open-science-dlt](https://github.com/dj-ccs/open-science-dlt)
- **Migration Date:** 2025-11-14
- **Purpose:** Separate simulation engine from DLT publishing layer

### Embedded Mathematics
- **Inspiration:** [Doom Engine](https://github.com/id-Software/DOOM) fixed-point arithmetic
- **License:** GPL-2.0 (for embedded/ directory)

---

## 🛠️ Development

### Install Development Dependencies

```bash
# Python
pip install -e ".[dev]"

# TypeScript
npm install --save-dev
```

### Run Linters and Formatters

```bash
# Python
black src/
ruff check src/
mypy src/

# TypeScript
npm run lint
npm run format
```

### Build TypeScript

```bash
npm run build
```

---

## 🤝 Contributing

We welcome contributions! Please:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

See [CONTRIBUTING.md](#) (coming soon) for detailed guidelines.

---

## 📄 License

**Dual-licensed** under your choice of:
- [MIT License](LICENSE) - Permissive, allows proprietary use
- [GNU GPL v3.0](LICENSE) - Copyleft, ensures derivatives remain open

Choose the license that best fits your use case. See [LICENSE](LICENSE) for full text and guidance.

---

## 🌐 Links

- **Homepage:** [negentropic-core.org](#) (coming soon)
- **Documentation:** [docs.negentropic-core.org](#) (coming soon)
- **Issues:** [GitHub Issues](https://github.com/your-org/negentropic-core/issues)
- **UCF Repository:** [github.com/dj-ccs/Unified-Conscious-Evolution-Framework](https://github.com/dj-ccs/Unified-Conscious-Evolution-Framework)
- **Open Science DLT:** [github.com/dj-ccs/open-science-dlt](https://github.com/dj-ccs/open-science-dlt)

---

## 🙏 Acknowledgments

This project integrates mathematical foundations from:
- **Unified Conscious Evolution Framework (UCF)** - SE(3) regenerative principles
- **Open Science DLT** - Original computational substrate
- **Eckmann & Tlusty (2025)** - Mathematical foundation (arXiv:2502.14367)
- **id Software** - Doom engine fixed-point arithmetic inspiration

Built with collaborative AI assistance: Claude, ChatGPT, Gemini, Edison Scientific, Grok.

---

**Version:** 0.1.0-alpha | **Status:** Initial Port | **Updated:** 2025-11-14
