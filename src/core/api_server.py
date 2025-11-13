"""
Flask API Server for SE(3) Regenerative Metrics Service

PROVENANCE:
-----------
Created for: Open Science DLT - Pillar I (Science)
Integration Date: 2025-11-11
Session: claude/se3-lambda-estimation-service-011CV1jDenzxAxddVhcat8uS
Purpose: REST API endpoint for λ-estimation and verification metrics

This Flask application exposes the SE(3) λ-estimation service via REST API,
enabling integration with the TypeScript/Node.js Open Science DLT platform.

API Endpoints:
--------------
POST /api/v1/science/metrics         - Compute regenerative metrics
POST /api/v1/science/metrics/batch   - Batch metrics computation
GET  /api/v1/science/health          - Health check
GET  /api/v1/science/version         - Service version info
"""

from flask import Flask, request, jsonify
from flask_cors import CORS
import logging
import sys
from pathlib import Path

# Add parent directory to path for imports
sys.path.insert(0, str(Path(__file__).parent))

from lie_dynamics.metrics_service import (
    compute_regenerative_metrics,
    compute_batch_metrics
)

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

# Create Flask app
app = Flask(__name__)

# Enable CORS for TypeScript/Node.js integration
CORS(app, resources={
    r"/api/*": {
        "origins": ["http://localhost:3000", "http://127.0.0.1:3000"],
        "methods": ["GET", "POST", "OPTIONS"],
        "allow_headers": ["Content-Type", "Authorization"]
    }
})

# Service version
SERVICE_VERSION = "1.0.0"
SERVICE_NAME = "SE(3) Regenerative Metrics Service"


@app.route('/api/v1/science/health', methods=['GET'])
def health_check():
    """
    Health check endpoint.

    Returns:
        JSON with service status
    """
    return jsonify({
        "status": "ok",
        "service": SERVICE_NAME,
        "version": SERVICE_VERSION,
        "timestamp": str(Path(__file__).stat().st_mtime)
    }), 200


@app.route('/api/v1/science/version', methods=['GET'])
def version_info():
    """
    Service version information.

    Returns:
        JSON with detailed version info
    """
    return jsonify({
        "service": SERVICE_NAME,
        "version": SERVICE_VERSION,
        "provenance": {
            "source": "Unified Conscious Evolution Framework (UCF)",
            "repository": "https://github.com/dj-ccs/Unified-Conscious-Evolution-Framework",
            "integration_date": "2025-11-11",
            "session": "claude/se3-lambda-estimation-service-011CV1jDenzxAxddVhcat8uS"
        },
        "mathematical_foundation": {
            "principle": "SE(3) Double-and-Scale Regenerative Return",
            "reference": "Eckmann & Tlusty (2025), arXiv:2502.14367",
            "adr": "ADR-0001"
        }
    }), 200


@app.route('/api/v1/science/metrics', methods=['POST'])
def compute_metrics():
    """
    Compute regenerative metrics from trajectory data.

    Expected JSON payload:
    {
        "trajectory_data": {
            "poses": [
                {"rotation": [x, y, z], "translation": [x, y, z]},
                ...
            ]
        },
        "options": {
            "enable_resonance_detection": true,
            "enable_verification_cascade": true,
            "bounded": true,
            "r_max": 1.0,
            "lambda_bounds": [0.1, 2.0]
        }
    }

    Returns:
        JSON with regenerative metrics:
        {
            "optimal_lambda": float,
            "return_error_epsilon": float,
            "verification_score": float,
            "resonance_detected": str (optional),
            "confidence": float,
            "metadata": {}
        }
    """
    try:
        # Parse request
        data = request.get_json()

        if not data:
            return jsonify({
                "error": "No JSON data provided",
                "status": "error"
            }), 400

        # Extract trajectory data
        trajectory_data = data.get('trajectory_data')
        if not trajectory_data:
            return jsonify({
                "error": "Missing 'trajectory_data' field",
                "status": "error"
            }), 400

        # Extract options (with defaults)
        options = data.get('options', {})
        enable_resonance = options.get('enable_resonance_detection', True)
        enable_verification = options.get('enable_verification_cascade', True)
        bounded = options.get('bounded', True)
        r_max = options.get('r_max', 1.0)
        lambda_bounds = tuple(options.get('lambda_bounds', [0.1, 2.0]))

        logger.info(f"Computing metrics for trajectory with {len(trajectory_data.get('poses', []))} poses")

        # Compute metrics
        metrics = compute_regenerative_metrics(
            trajectory_data,
            enable_resonance_detection=enable_resonance,
            enable_verification_cascade=enable_verification,
            bounded=bounded,
            r_max=r_max,
            lambda_bounds=lambda_bounds
        )

        logger.info(f"Metrics computed successfully: λ={metrics['optimal_lambda']:.4f}")

        return jsonify({
            "status": "success",
            "data": metrics
        }), 200

    except ValueError as e:
        logger.error(f"Validation error: {str(e)}")
        return jsonify({
            "error": str(e),
            "status": "validation_error"
        }), 400

    except Exception as e:
        logger.error(f"Internal error: {str(e)}", exc_info=True)
        return jsonify({
            "error": "Internal server error",
            "message": str(e),
            "status": "error"
        }), 500


@app.route('/api/v1/science/metrics/batch', methods=['POST'])
def compute_metrics_batch():
    """
    Compute regenerative metrics for multiple trajectories in batch.

    Expected JSON payload:
    {
        "trajectories": [
            {"poses": [...]},
            {"poses": [...]},
            ...
        ],
        "options": {...}
    }

    Returns:
        JSON with array of metrics results
    """
    try:
        data = request.get_json()

        if not data:
            return jsonify({
                "error": "No JSON data provided",
                "status": "error"
            }), 400

        trajectories = data.get('trajectories')
        if not trajectories:
            return jsonify({
                "error": "Missing 'trajectories' field",
                "status": "error"
            }), 400

        options = data.get('options', {})

        logger.info(f"Computing metrics for {len(trajectories)} trajectories")

        results = compute_batch_metrics(
            trajectories,
            enable_resonance_detection=options.get('enable_resonance_detection', True),
            enable_verification_cascade=options.get('enable_verification_cascade', True),
            bounded=options.get('bounded', True),
            r_max=options.get('r_max', 1.0),
            lambda_bounds=tuple(options.get('lambda_bounds', [0.1, 2.0]))
        )

        return jsonify({
            "status": "success",
            "data": results,
            "count": len(results)
        }), 200

    except Exception as e:
        logger.error(f"Batch processing error: {str(e)}", exc_info=True)
        return jsonify({
            "error": "Batch processing failed",
            "message": str(e),
            "status": "error"
        }), 500


@app.errorhandler(404)
def not_found(error):
    """Handle 404 errors"""
    return jsonify({
        "error": "Endpoint not found",
        "status": "not_found"
    }), 404


@app.errorhandler(500)
def internal_error(error):
    """Handle 500 errors"""
    return jsonify({
        "error": "Internal server error",
        "status": "error"
    }), 500


def main():
    """Start the Flask server"""
    import os

    host = os.environ.get('SCIENCE_API_HOST', '127.0.0.1')
    port = int(os.environ.get('SCIENCE_API_PORT', 5000))
    debug = os.environ.get('SCIENCE_API_DEBUG', 'false').lower() == 'true'

    logger.info(f"Starting {SERVICE_NAME} v{SERVICE_VERSION}")
    logger.info(f"Server listening on {host}:{port}")

    app.run(
        host=host,
        port=port,
        debug=debug
    )


if __name__ == '__main__':
    main()
