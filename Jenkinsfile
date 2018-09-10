pipeline {
  agent any
  stages {
    stage('Build') {
      parallel {
        stage('Build') {
          steps {
            sh '''echo "Building"
echo "Build OK"'''
          }
        }
        stage('Build Parallel') {
          steps {
            sh '''echo "Build in parallel"
echo "Build in parallel - OK"'''
          }
        }
      }
    }
    stage('Test') {
      steps {
        sh '''echo "Testing"
echo "Test OK"'''
      }
    }
    stage('Deploy') {
      steps {
        sh '''echo "Deploying"
echo "Deploy OK"'''
      }
    }
  }
}